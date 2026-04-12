# OTel collector example

Minimal [OpenTelemetry Collector](https://opentelemetry.io/docs/collector/) setup for local development with `phenotype-otel.js`.

## Quick start

```bash
# 1. Start the collector (OTLP/HTTP on :4318, debug exporter → stdout)
docker compose up -d

# 2. Tail the collector output — you'll see OTLP payloads here
docker compose logs -f collector
```

Then wire the adapter into your phenotype app:

```js
import { mount } from './phenotype.js';
import { mountOtel } from './phenotype-otel.js';

const phenotype = await mount('app.wasm');
mountOtel({
  phenotype,
  metricsEndpoint: 'http://localhost:4318/v1/metrics',
  logsEndpoint:    'http://localhost:4318/v1/logs',
  intervalMs:      5000,
});
```

Within 5 seconds the collector logs should print `ResourceMetrics` and `ResourceLogs` payloads listing every instrument defined in `phenotype.diag`:

- **Counters**: `phenotype.runner.rebuilds`, `phenotype.host.flush_calls`, `phenotype.runner.frames_skipped`, `phenotype.input.events`, ...
- **Gauges**: `phenotype.arena.live_nodes`, `phenotype.arena.max_nodes_seen`, ...
- **Histograms**: `phenotype.runner.frame_duration`, `phenotype.runner.phase_duration`
- **Logs**: runner debug lines, input trace lines, warnings

## CORS

The `collector-config.yaml` allows origins `localhost:8765` and `127.0.0.1:8765`. If your dev server runs on a different port, add it to the `cors.allowed_origins` list — without a matching origin the browser silently blocks every POST.

## Stopping

```bash
docker compose down
```

Call the `stop()` function returned by `mountOtel()` to stop JS-side polling.

## Next steps

The `debug` exporter just prints to stdout. To visualize metrics in Grafana:

1. Replace the `debug` exporter with `prometheus` in `collector-config.yaml`
2. Add a Prometheus service that scrapes the collector
3. Add a Grafana service with the Prometheus datasource provisioned

See the [OTel Collector docs](https://opentelemetry.io/docs/collector/configuration/) for exporter configuration.

For hosted backends (Grafana Cloud, Honeycomb, Datadog), swap the endpoint and add auth headers — no code change needed:

```js
mountOtel({
  phenotype,
  metricsEndpoint: 'https://otel.vendor.com/v1/metrics',
  logsEndpoint:    'https://otel.vendor.com/v1/logs',
  headers: { 'Authorization': 'Bearer YOUR_API_KEY' },
});
```
