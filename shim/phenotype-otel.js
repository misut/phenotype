// phenotype-otel.js — OpenTelemetry adapter for phenotype.diag.
//
// Polls phenotype.diagExport() on a timer, transforms the project-
// internal snapshot into OTLP/HTTP JSON envelopes, and POSTs them to
// a user-configured collector. Opt-in: apps call mountOtel() after
// mount() resolves.
//
// Usage:
//   import { mount } from './phenotype.js';
//   import { mountOtel } from './phenotype-otel.js';
//
//   const phenotype = await mount('app.wasm');
//   const stop = mountOtel({
//     phenotype,
//     metricsEndpoint: 'http://localhost:4318/v1/metrics',
//     logsEndpoint:    'http://localhost:4318/v1/logs',
//   });
//
// Transformation notes:
//   * The project snapshot uses snake_case fields (time_unix_nano,
//     data_points, ...) and stores int64 values as raw JS numbers.
//     OTLP/HTTP JSON uses camelCase (timeUnixNano, dataPoints) and
//     quotes int64 values as strings. This module does that mapping.
//   * Counters / gauges / histograms are cumulative on the C++ side.
//     OTLP aggregationTemporality 2 (CUMULATIVE) is set on every
//     metric so the collector knows to compute deltas.
//   * Logs live in a 256-entry ring buffer that is NOT drained by
//     phenotype_diag_export(). We dedupe on the JS side by tracking
//     the highest time_unix_nano already sent and skipping anything
//     <= it on the next poll.

const DEFAULTS = {
  metricsEndpoint:    'http://localhost:4318/v1/metrics',
  logsEndpoint:       'http://localhost:4318/v1/logs',
  headers:            {},
  intervalMs:         10000,
  serviceName:        'phenotype-app',
  serviceVersion:     null,
  resourceAttributes: {},
  onError:            (err) => console.warn('[phenotype-otel]', err),
};

// ---- transformation helpers ----

function attrsToOtlp(obj) {
  const out = [];
  for (const [k, v] of Object.entries(obj || {})) {
    if (typeof v === 'string')
      out.push({ key: k, value: { stringValue: v } });
    else if (typeof v === 'number' && Number.isInteger(v))
      out.push({ key: k, value: { intValue: String(v) } });
    else if (typeof v === 'number')
      out.push({ key: k, value: { doubleValue: v } });
    else if (typeof v === 'boolean')
      out.push({ key: k, value: { boolValue: v } });
    else
      out.push({ key: k, value: { stringValue: String(v) } });
  }
  return out;
}

function resourceToOtlp(snapshot, opts) {
  const merged = {
    'service.name':    opts.serviceName,
    'service.version': opts.serviceVersion
      ?? snapshot.resource?.['service.version'] ?? 'unknown',
    'runtime.name': snapshot.resource?.['runtime.name'] ?? 'unknown',
    ...opts.resourceAttributes,
  };
  return { attributes: attrsToOtlp(merged) };
}

function counterToOtlp(c) {
  return {
    name:        c.name,
    description: c.description,
    unit:        c.unit,
    sum: {
      aggregationTemporality: 2,
      isMonotonic: c.is_monotonic,
      dataPoints: c.data_points.map(dp => ({
        attributes:        attrsToOtlp(dp.attributes),
        startTimeUnixNano: String(dp.start_time_unix_nano),
        timeUnixNano:      String(dp.time_unix_nano),
        asInt:             String(dp.value),
      })),
    },
  };
}

function gaugeToOtlp(g) {
  return {
    name:        g.name,
    description: g.description,
    unit:        g.unit,
    gauge: {
      dataPoints: g.data_points.map(dp => ({
        attributes:   attrsToOtlp(dp.attributes),
        timeUnixNano: String(dp.time_unix_nano),
        asInt:        String(dp.value),
      })),
    },
  };
}

function histogramToOtlp(h) {
  return {
    name:        h.name,
    description: h.description,
    unit:        h.unit,
    histogram: {
      aggregationTemporality: 2,
      dataPoints: h.data_points.map(dp => ({
        attributes:        attrsToOtlp(dp.attributes),
        startTimeUnixNano: String(dp.start_time_unix_nano),
        timeUnixNano:      String(dp.time_unix_nano),
        count:             String(dp.count),
        sum:               dp.sum,
        bucketCounts:      dp.bucket_counts.map(String),
        explicitBounds:    h.explicit_bounds,
      })),
    },
  };
}

function snapshotToOtlpMetrics(snapshot, opts) {
  const metrics = [
    ...snapshot.counters.map(counterToOtlp),
    ...snapshot.gauges.map(gaugeToOtlp),
    ...snapshot.histograms.map(histogramToOtlp),
  ];
  return {
    resourceMetrics: [{
      resource: resourceToOtlp(snapshot, opts),
      scopeMetrics: [{
        scope: {
          name:    'phenotype',
          version: snapshot.resource?.['service.version'] ?? 'unknown',
        },
        metrics,
      }],
    }],
  };
}

function snapshotToOtlpLogs(snapshot, opts, newLogs) {
  return {
    resourceLogs: [{
      resource: resourceToOtlp(snapshot, opts),
      scopeLogs: [{
        scope: {
          name:    'phenotype',
          version: snapshot.resource?.['service.version'] ?? 'unknown',
        },
        logRecords: newLogs.map(rec => ({
          timeUnixNano:   String(rec.time_unix_nano),
          severityNumber: rec.severity_number,
          severityText:   rec.severity_text,
          body:           { stringValue: rec.body },
          attributes:     attrsToOtlp({ scope: rec.scope_name }),
        })),
      }],
    }],
  };
}

// ---- public API ----

export function mountOtel(userOpts) {
  const opts = { ...DEFAULTS, ...userOpts };
  const phenotype = opts.phenotype;
  if (!phenotype || typeof phenotype.diagExport !== 'function') {
    throw new Error(
      '[phenotype-otel] mountOtel requires { phenotype } '
      + '— pass the handle returned by mount()'
    );
  }

  let lastLogTimeNano = 0;
  let stopped = false;

  async function postOtlp(endpoint, body) {
    try {
      const res = await fetch(endpoint, {
        method:  'POST',
        headers: { 'Content-Type': 'application/json', ...opts.headers },
        body:    JSON.stringify(body),
        keepalive: true,
      });
      if (!res.ok) {
        opts.onError(
          new Error(`POST ${endpoint} -> ${res.status} ${res.statusText}`)
        );
      }
    } catch (err) {
      opts.onError(err);
    }
  }

  async function pollOnce() {
    if (stopped) return;
    let snapshot;
    try {
      snapshot = phenotype.diagExport();
    } catch (err) {
      opts.onError(err);
      return;
    }
    if (!snapshot) return;

    // Metrics — push every poll; the collector handles cumulative dedup.
    const metricsBody = snapshotToOtlpMetrics(snapshot, opts);
    postOtlp(opts.metricsEndpoint, metricsBody);

    // Logs — only the ones newer than what we already sent.
    const newLogs = (snapshot.logs || []).filter(
      rec => rec.time_unix_nano > lastLogTimeNano
    );
    if (newLogs.length > 0) {
      lastLogTimeNano = newLogs[newLogs.length - 1].time_unix_nano;
      const logsBody = snapshotToOtlpLogs(snapshot, opts, newLogs);
      postOtlp(opts.logsEndpoint, logsBody);
    }
  }

  // First poll immediately so configuration errors surface fast.
  pollOnce();
  const handle = setInterval(pollOnce, opts.intervalMs);

  return function stop() {
    stopped = true;
    clearInterval(handle);
  };
}
