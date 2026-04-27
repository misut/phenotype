#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

import phenotype;
import phenotype.native;

namespace {

using phenotype::Color;

constexpr float kRouteCanvasWidth = 382.0f;
constexpr float kRouteCanvasHeight = 244.0f;
constexpr float kMediaCanvasWidth = 300.0f;
constexpr float kMediaCanvasHeight = 154.0f;
constexpr float kTimelineWidth = 378.0f;
constexpr float kTimelineHeight = 86.0f;

struct Point {
    float x;
    float y;
};

enum class FlightStatus {
    Boarding,
    Taxiing,
    Departed,
    Delayed,
    Landed,
    Maintenance,
};

enum class Severity {
    Info,
    Success,
    Warning,
    Critical,
};

enum class FlightAction {
    Delay,
    Board,
    Depart,
    Land,
    Reset,
};

enum class IconKind {
    Airport,
    Clock,
    Flight,
    Delay,
    Weather,
    Ops,
    Boarding,
    Taxiing,
    Departed,
    Landed,
    Maintenance,
    Info,
    Success,
    Warning,
    Critical,
    Reset,
};

struct DestinationVisual {
    char const* city;
    char const* weather;
    char const* temp;
    char const* terminal_note;
    Color sky_top;
    Color sky_bottom;
    Color accent;
};

struct Flight {
    int id;
    char const* number;
    char const* origin;
    char const* destination;
    char const* gate;
    char const* aircraft;
    char const* crew;
    char const* baggage_belt;
    char const* tail_number;
    FlightStatus status;
    char const* scheduled;
    char const* eta_etd;
    int delay_minutes;
    float progress;
    std::array<Point, 3> route_points;
    DestinationVisual visual;
};

struct FeedItem {
    Severity severity;
    std::string time;
    std::string title;
    std::string detail;
};

struct SelectFlight {
    int id;
};

struct ApplyFlightAction {
    FlightAction action;
};

using Msg = std::variant<SelectFlight, ApplyFlightAction>;

std::string local_time_short() {
    auto now = std::chrono::system_clock::now();
    auto raw = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &raw);
#else
    localtime_r(&raw, &tm);
#endif
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    return buf;
}

Color rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return {r, g, b, a};
}

Color mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto channel = [t](unsigned char x, unsigned char y) -> unsigned char {
        return static_cast<unsigned char>(
            static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
}

Color status_color(FlightStatus status) {
    switch (status) {
    case FlightStatus::Boarding:    return rgba(20, 184, 166);
    case FlightStatus::Taxiing:     return rgba(99, 102, 241);
    case FlightStatus::Departed:    return rgba(37, 99, 235);
    case FlightStatus::Delayed:     return rgba(245, 158, 11);
    case FlightStatus::Landed:      return rgba(34, 197, 94);
    case FlightStatus::Maintenance: return rgba(220, 38, 38);
    }
    return rgba(107, 114, 128);
}

Color status_bg(FlightStatus status) {
    switch (status) {
    case FlightStatus::Boarding:    return rgba(204, 251, 241);
    case FlightStatus::Taxiing:     return rgba(224, 231, 255);
    case FlightStatus::Departed:    return rgba(219, 234, 254);
    case FlightStatus::Delayed:     return rgba(254, 243, 199);
    case FlightStatus::Landed:      return rgba(220, 252, 231);
    case FlightStatus::Maintenance: return rgba(254, 226, 226);
    }
    return rgba(243, 244, 246);
}

char const* status_label(FlightStatus status) {
    switch (status) {
    case FlightStatus::Boarding:    return "boarding";
    case FlightStatus::Taxiing:     return "taxiing";
    case FlightStatus::Departed:    return "departed";
    case FlightStatus::Delayed:     return "delayed";
    case FlightStatus::Landed:      return "landed";
    case FlightStatus::Maintenance: return "maint";
    }
    return "unknown";
}

IconKind status_icon_kind(FlightStatus status) {
    switch (status) {
    case FlightStatus::Boarding:    return IconKind::Boarding;
    case FlightStatus::Taxiing:     return IconKind::Taxiing;
    case FlightStatus::Departed:    return IconKind::Departed;
    case FlightStatus::Delayed:     return IconKind::Delay;
    case FlightStatus::Landed:      return IconKind::Landed;
    case FlightStatus::Maintenance: return IconKind::Maintenance;
    }
    return IconKind::Info;
}

Color severity_color(Severity severity) {
    switch (severity) {
    case Severity::Success:  return rgba(22, 163, 74);
    case Severity::Warning:  return rgba(217, 119, 6);
    case Severity::Critical: return rgba(220, 38, 38);
    case Severity::Info:
    default:                 return rgba(37, 99, 235);
    }
}

Color severity_bg(Severity severity) {
    switch (severity) {
    case Severity::Success:  return rgba(220, 252, 231);
    case Severity::Warning:  return rgba(254, 243, 199);
    case Severity::Critical: return rgba(254, 226, 226);
    case Severity::Info:
    default:                 return rgba(219, 234, 254);
    }
}

IconKind severity_icon_kind(Severity severity) {
    switch (severity) {
    case Severity::Success:  return IconKind::Success;
    case Severity::Warning:  return IconKind::Warning;
    case Severity::Critical: return IconKind::Critical;
    case Severity::Info:
    default:                 return IconKind::Info;
    }
}

IconKind action_icon_kind(FlightAction action) {
    switch (action) {
    case FlightAction::Delay:  return IconKind::Delay;
    case FlightAction::Board:  return IconKind::Boarding;
    case FlightAction::Depart: return IconKind::Departed;
    case FlightAction::Land:   return IconKind::Landed;
    case FlightAction::Reset:  return IconKind::Reset;
    }
    return IconKind::Info;
}

std::vector<Flight> make_initial_flights() {
    return {
        {
            101, "KE 017", "ICN", "LAX", "A12", "B789",
            "Crew 6A", "Belt 4", "HL8083", FlightStatus::Boarding,
            "14:10", "ETD 14:25", 15, 0.32f,
            {{{58, 180}, {172, 54}, {336, 104}}},
            {"Los Angeles", "Clear", "23C", "Tom Bradley arrivals holding at belt 4",
             rgba(58, 123, 213), rgba(157, 196, 255), rgba(251, 191, 36)}
        },
        {
            118, "OZ 742", "BKK", "ICN", "B07", "A333",
            "Crew 2C", "Belt 11", "HL7792", FlightStatus::Landed,
            "13:35", "ETA 13:41", 6, 1.0f,
            {{{334, 190}, {230, 78}, {78, 110}}},
            {"Seoul", "Clouds", "18C", "Transfer desk open near terminal 2",
             rgba(96, 165, 250), rgba(226, 232, 240), rgba(34, 197, 94)}
        },
        {
            134, "JL 094", "HND", "GMP", "C03", "B788",
            "Crew 4B", "Belt 2", "JA834J", FlightStatus::Taxiing,
            "14:00", "ETD 14:08", 0, 0.46f,
            {{{326, 126}, {248, 68}, {96, 148}}},
            {"Seoul Gimpo", "Wind", "17C", "Crosswind watch active on short final",
             rgba(14, 165, 233), rgba(186, 230, 253), rgba(99, 102, 241)}
        },
        {
            152, "AF 267", "ICN", "CDG", "D18", "A359",
            "Crew 8F", "Belt 6", "F-HTYH", FlightStatus::Delayed,
            "14:45", "ETD 15:25", 40, 0.20f,
            {{{70, 170}, {162, 40}, {318, 72}}},
            {"Paris", "Rain", "12C", "Gate D18 catering truck rescheduled",
             rgba(71, 85, 105), rgba(203, 213, 225), rgba(245, 158, 11)}
        },
        {
            173, "UA 892", "SFO", "ICN", "E02", "B77W",
            "Crew 9D", "Belt 8", "N2749U", FlightStatus::Departed,
            "13:55", "ETA 17:20", 0, 0.67f,
            {{{336, 98}, {190, 32}, {62, 176}}},
            {"San Francisco", "Fog", "14C", "Arrival metering over Pacific track",
             rgba(100, 116, 139), rgba(226, 232, 240), rgba(20, 184, 166)}
        },
        {
            196, "TW 301", "ICN", "NRT", "M05", "B738",
            "Crew 1A", "Belt 1", "HL8321", FlightStatus::Maintenance,
            "15:05", "ETD 16:05", 60, 0.08f,
            {{{82, 164}, {184, 88}, {326, 138}}},
            {"Tokyo", "Clear", "20C", "Maintenance release pending avionics check",
             rgba(125, 211, 252), rgba(219, 234, 254), rgba(220, 38, 38)}
        },
    };
}

std::vector<FeedItem> make_initial_feed() {
    return {
        {Severity::Warning, "13:58", "AF 267 delay alert", "Catering truck moved"},
        {Severity::Success, "13:49", "OZ 742 landed", "Belt 11 open"},
        {Severity::Info, "13:42", "KE 017 boarding", "Group 2 scanning"},
        {Severity::Info, "13:31", "UA 892 departed", "Oceanic clearance"},
    };
}

struct State {
    int selected_id = 101;
    int animation_tick = 0;
    int revision = 0;
    std::vector<Flight> flights = make_initial_flights();
    std::vector<FeedItem> feed = make_initial_feed();
    std::string ops_mode = "Peak turnaround";
    std::string weather = "VFR 18C wind 07kt";
};

Flight* find_flight(State& state, int id) {
    auto it = std::find_if(state.flights.begin(), state.flights.end(),
        [id](Flight const& flight) { return flight.id == id; });
    return it == state.flights.end() ? nullptr : &*it;
}

Flight const& selected_flight(State const& state) {
    auto it = std::find_if(state.flights.begin(), state.flights.end(),
        [&](Flight const& flight) { return flight.id == state.selected_id; });
    return it == state.flights.end() ? state.flights.front() : *it;
}

int active_flight_count(State const& state) {
    int count = 0;
    for (auto const& flight : state.flights) {
        if (flight.status != FlightStatus::Landed
            && flight.status != FlightStatus::Maintenance) {
            ++count;
        }
    }
    return count;
}

int delayed_flight_count(State const& state) {
    int count = 0;
    for (auto const& flight : state.flights) {
        if (flight.status == FlightStatus::Delayed || flight.delay_minutes > 0)
            ++count;
    }
    return count;
}

std::string route_text(Flight const& flight) {
    std::string out = flight.origin;
    out += "-";
    out += flight.destination;
    return out;
}

std::string delay_text(Flight const& flight) {
    if (flight.delay_minutes <= 0)
        return "on time";
    return "+" + std::to_string(flight.delay_minutes) + "m";
}

std::string percent_text(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return std::to_string(static_cast<int>(std::round(value * 100.0f))) + "%";
}

std::string trunc(std::string text, std::size_t max_len) {
    if (text.size() <= max_len)
        return text;
    if (max_len <= 1)
        return text.substr(0, max_len);
    return text.substr(0, max_len - 1) + ".";
}

std::string flight_summary(Flight const& flight) {
    std::string out = flight.number;
    out += " ";
    out += route_text(flight);
    out += " ";
    out += status_label(flight.status);
    return out;
}

void push_feed(State& state, Severity severity, std::string title, std::string detail) {
    state.feed.insert(state.feed.begin(), FeedItem{
        severity,
        local_time_short(),
        std::move(title),
        std::move(detail),
    });
    if (state.feed.size() > 4)
        state.feed.resize(4);
}

void update(State& state, Msg msg) {
    std::visit([&](auto const& event) {
        using T = std::decay_t<decltype(event)>;
        if constexpr (std::same_as<T, SelectFlight>) {
            state.selected_id = event.id;
            auto const& flight = selected_flight(state);
            push_feed(state, Severity::Info,
                std::string("Tracking ") + flight.number,
                route_text(flight) + " selected");
            ++state.animation_tick;
            ++state.revision;
        } else if constexpr (std::same_as<T, ApplyFlightAction>) {
            if (event.action == FlightAction::Reset) {
                state.flights = make_initial_flights();
                state.selected_id = 101;
                state.feed = make_initial_feed();
                push_feed(state, Severity::Info,
                    "Scenario reset",
                    "Baseline restored");
                ++state.animation_tick;
                ++state.revision;
                return;
            }

            auto* flight = find_flight(state, state.selected_id);
            if (!flight)
                return;

            if (event.action == FlightAction::Delay) {
                flight->status = FlightStatus::Delayed;
                flight->delay_minutes = std::min(120, flight->delay_minutes + 15);
                flight->progress = std::min(flight->progress, 0.38f);
                flight->eta_etd = "ETD revised";
                push_feed(state, Severity::Warning,
                    std::string(flight->number) + " delay",
                    "+" + std::to_string(flight->delay_minutes) + "m across board");
            } else if (event.action == FlightAction::Board) {
                flight->status = FlightStatus::Boarding;
                flight->progress = std::max(flight->progress, 0.30f);
                flight->eta_etd = "Boarding now";
                push_feed(state, Severity::Info,
                    std::string(flight->number) + " boarding",
                    std::string("Gate ") + flight->gate + " open");
            } else if (event.action == FlightAction::Depart) {
                flight->status = FlightStatus::Departed;
                flight->delay_minutes = std::max(0, flight->delay_minutes - 5);
                flight->progress = std::max(flight->progress, 0.64f);
                flight->eta_etd = "Airborne";
                push_feed(state, Severity::Success,
                    std::string(flight->number) + " departed",
                    std::string(flight->aircraft) + " airborne");
            } else if (event.action == FlightAction::Land) {
                flight->status = FlightStatus::Landed;
                flight->progress = 1.0f;
                flight->eta_etd = "Arrived";
                flight->delay_minutes = std::max(0, flight->delay_minutes);
                push_feed(state, Severity::Success,
                    std::string(flight->number) + " landed",
                    std::string(flight->baggage_belt) + " assigned");
            }

            ++state.animation_tick;
            ++state.revision;
        }
    }, msg);
}

void fill_rect(phenotype::Painter& painter,
               float x,
               float y,
               float w,
               float h,
               Color color) {
    if (w <= 0.0f || h <= 0.0f)
        return;
    painter.line(x, y + h * 0.5f, x + w, y + h * 0.5f, h, color);
}

void fill_gradient(phenotype::Painter& painter,
                   float x,
                   float y,
                   float w,
                   float h,
                   Color top,
                   Color bottom,
                   int bands = 18) {
    bands = std::max(1, bands);
    auto band_h = h / static_cast<float>(bands);
    for (int i = 0; i < bands; ++i) {
        auto t = bands == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(bands - 1);
        fill_rect(painter, x, y + band_h * static_cast<float>(i),
                  w, band_h + 0.75f, mix(top, bottom, t));
    }
}

void stroke_rect(phenotype::Painter& painter,
                 float x,
                 float y,
                 float w,
                 float h,
                 Color color,
                 float thickness = 1.0f) {
    painter.line(x, y, x + w, y, thickness, color);
    painter.line(x + w, y, x + w, y + h, thickness, color);
    painter.line(x + w, y + h, x, y + h, thickness, color);
    painter.line(x, y + h, x, y, thickness, color);
}

void fill_circle(phenotype::Painter& painter,
                 float cx,
                 float cy,
                 float radius,
                 Color color) {
    int r = static_cast<int>(std::ceil(radius));
    for (int y = -r; y <= r; ++y) {
        auto fy = static_cast<float>(y);
        auto span_sq = radius * radius - fy * fy;
        if (span_sq < 0.0f)
            continue;
        auto span = std::sqrt(span_sq);
        painter.line(cx - span, cy + fy, cx + span, cy + fy, 1.0f, color);
    }
}

void stroke_circle(phenotype::Painter& painter,
                   float cx,
                   float cy,
                   float radius,
                   Color color,
                   float thickness = 1.0f) {
    constexpr int kSegments = 48;
    Point prev{cx + radius, cy};
    for (int i = 1; i <= kSegments; ++i) {
        auto a = static_cast<float>(i) / static_cast<float>(kSegments) * 6.2831853f;
        Point next{cx + std::cos(a) * radius, cy + std::sin(a) * radius};
        painter.line(prev.x, prev.y, next.x, next.y, thickness, color);
        prev = next;
    }
}

void draw_text(phenotype::Painter& painter,
               float x,
               float y,
               std::string const& text,
               float size,
               Color color) {
    painter.text(x, y, text.c_str(), static_cast<unsigned int>(text.size()), size, color);
}

void draw_text_view(phenotype::Painter& painter,
                    float x,
                    float y,
                    std::string_view text,
                    float size,
                    Color color) {
    painter.text(x, y, text.data(), static_cast<unsigned int>(text.size()), size, color);
}

Point icon_point(float cx, float cy, float size, float x, float y) {
    auto s = size / 24.0f;
    return {cx + (x - 12.0f) * s, cy + (y - 12.0f) * s};
}

void icon_line(phenotype::Painter& painter,
               float cx,
               float cy,
               float size,
               float ax,
               float ay,
               float bx,
               float by,
               Color color,
               float weight = 1.5f) {
    auto a = icon_point(cx, cy, size, ax, ay);
    auto b = icon_point(cx, cy, size, bx, by);
    painter.line(a.x, a.y, b.x, b.y, std::max(1.0f, size / 24.0f * weight), color);
}

void icon_rect(phenotype::Painter& painter,
               float cx,
               float cy,
               float size,
               float x,
               float y,
               float w,
               float h,
               Color color,
               float weight = 1.5f) {
    auto p = icon_point(cx, cy, size, x, y);
    auto s = size / 24.0f;
    stroke_rect(painter, p.x, p.y, w * s, h * s, color, std::max(1.0f, s * weight));
}

void icon_circle(phenotype::Painter& painter,
                 float cx,
                 float cy,
                 float size,
                 float x,
                 float y,
                 float r,
                 Color color,
                 float weight = 1.5f) {
    auto p = icon_point(cx, cy, size, x, y);
    auto s = size / 24.0f;
    auto radius = r * s;
    auto thickness = std::max(1.0f, s * weight);
    constexpr int kIconSegments = 16;
    Point prev{p.x + radius, p.y};
    for (int i = 1; i <= kIconSegments; ++i) {
        auto a = static_cast<float>(i) / static_cast<float>(kIconSegments) * 6.2831853f;
        Point next{p.x + std::cos(a) * radius, p.y + std::sin(a) * radius};
        painter.line(prev.x, prev.y, next.x, next.y, thickness, color);
        prev = next;
    }
}

void draw_icon(phenotype::Painter& painter,
               IconKind kind,
               float cx,
               float cy,
               float size,
               Color color) {
    switch (kind) {
    case IconKind::Airport:
        icon_rect(painter, cx, cy, size, 5, 11, 14, 8, color);
        icon_line(painter, cx, cy, size, 12, 5, 12, 20, color);
        icon_line(painter, cx, cy, size, 8, 10, 12, 5, color);
        icon_line(painter, cx, cy, size, 12, 5, 16, 10, color);
        icon_line(painter, cx, cy, size, 8, 14, 16, 14, color);
        icon_line(painter, cx, cy, size, 9, 17, 15, 17, color);
        break;
    case IconKind::Clock:
        icon_circle(painter, cx, cy, size, 12, 12, 8, color);
        icon_line(painter, cx, cy, size, 12, 12, 12, 7, color);
        icon_line(painter, cx, cy, size, 12, 12, 16, 14, color);
        break;
    case IconKind::Flight:
        icon_line(painter, cx, cy, size, 4, 13, 20, 10, color);
        icon_line(painter, cx, cy, size, 10, 12, 7, 6, color);
        icon_line(painter, cx, cy, size, 10, 12, 8, 18, color);
        icon_line(painter, cx, cy, size, 5, 13, 3, 10, color);
        icon_line(painter, cx, cy, size, 5, 13, 4, 16, color);
        break;
    case IconKind::Delay:
        icon_circle(painter, cx, cy, size, 11, 11, 6, color);
        icon_line(painter, cx, cy, size, 11, 11, 11, 7, color);
        icon_line(painter, cx, cy, size, 11, 11, 14, 13, color);
        icon_line(painter, cx, cy, size, 18, 7, 18, 15, color);
        icon_line(painter, cx, cy, size, 18, 18, 18, 18.2f, color, 2.0f);
        break;
    case IconKind::Weather:
        icon_circle(painter, cx, cy, size, 17, 7, 4, color);
        icon_line(painter, cx, cy, size, 17, 1.5f, 17, 3, color);
        icon_line(painter, cx, cy, size, 21, 3, 20, 4, color);
        icon_line(painter, cx, cy, size, 6, 16, 19, 16, color);
        icon_circle(painter, cx, cy, size, 8, 14, 3, color);
        icon_circle(painter, cx, cy, size, 12, 12, 4, color);
        icon_circle(painter, cx, cy, size, 16, 14, 3, color);
        break;
    case IconKind::Ops:
        icon_circle(painter, cx, cy, size, 12, 12, 3, color);
        icon_circle(painter, cx, cy, size, 12, 12, 8, color);
        icon_line(painter, cx, cy, size, 12, 3, 12, 6, color);
        icon_line(painter, cx, cy, size, 12, 18, 12, 21, color);
        icon_line(painter, cx, cy, size, 3, 12, 6, 12, color);
        icon_line(painter, cx, cy, size, 18, 12, 21, 12, color);
        icon_line(painter, cx, cy, size, 5.6f, 5.6f, 7.7f, 7.7f, color);
        icon_line(painter, cx, cy, size, 16.3f, 16.3f, 18.4f, 18.4f, color);
        icon_line(painter, cx, cy, size, 5.6f, 18.4f, 7.7f, 16.3f, color);
        icon_line(painter, cx, cy, size, 16.3f, 7.7f, 18.4f, 5.6f, color);
        break;
    case IconKind::Boarding:
        icon_rect(painter, cx, cy, size, 6, 4, 9, 16, color);
        icon_line(painter, cx, cy, size, 10, 12, 20, 12, color);
        icon_line(painter, cx, cy, size, 16, 8, 20, 12, color);
        icon_line(painter, cx, cy, size, 16, 16, 20, 12, color);
        break;
    case IconKind::Taxiing:
        icon_line(painter, cx, cy, size, 4, 18, 20, 18, color);
        icon_line(painter, cx, cy, size, 7, 12, 19, 12, color);
        icon_line(painter, cx, cy, size, 11, 12, 8, 7, color);
        icon_line(painter, cx, cy, size, 11, 12, 9, 17, color);
        icon_line(painter, cx, cy, size, 7, 12, 5, 10, color);
        break;
    case IconKind::Departed:
        icon_line(painter, cx, cy, size, 5, 18, 20, 18, color);
        icon_line(painter, cx, cy, size, 5, 16, 20, 8, color);
        icon_line(painter, cx, cy, size, 11, 13, 8, 8, color);
        icon_line(painter, cx, cy, size, 11, 13, 10, 19, color);
        icon_line(painter, cx, cy, size, 5, 16, 3, 13, color);
        break;
    case IconKind::Landed:
        icon_line(painter, cx, cy, size, 5, 19, 20, 19, color);
        icon_line(painter, cx, cy, size, 5, 8, 20, 16, color);
        icon_line(painter, cx, cy, size, 11, 11, 8, 16, color);
        icon_line(painter, cx, cy, size, 11, 11, 10, 5, color);
        icon_line(painter, cx, cy, size, 5, 8, 3, 11, color);
        break;
    case IconKind::Maintenance:
        icon_line(painter, cx, cy, size, 7, 18, 18, 7, color);
        icon_line(painter, cx, cy, size, 15, 4, 20, 4, color);
        icon_line(painter, cx, cy, size, 20, 4, 20, 9, color);
        icon_line(painter, cx, cy, size, 15, 4, 18, 7, color);
        icon_circle(painter, cx, cy, size, 7, 18, 2, color);
        break;
    case IconKind::Info:
        icon_circle(painter, cx, cy, size, 12, 12, 8, color);
        icon_line(painter, cx, cy, size, 12, 11, 12, 16, color);
        icon_line(painter, cx, cy, size, 12, 8, 12, 8.2f, color, 2.0f);
        break;
    case IconKind::Success:
        icon_line(painter, cx, cy, size, 5, 12.5f, 9.5f, 17, color);
        icon_line(painter, cx, cy, size, 9.5f, 17, 19, 7.5f, color);
        break;
    case IconKind::Warning:
        icon_line(painter, cx, cy, size, 12, 4, 21, 20, color);
        icon_line(painter, cx, cy, size, 21, 20, 3, 20, color);
        icon_line(painter, cx, cy, size, 3, 20, 12, 4, color);
        icon_line(painter, cx, cy, size, 12, 10, 12, 15, color);
        icon_line(painter, cx, cy, size, 12, 18, 12, 18.2f, color, 2.0f);
        break;
    case IconKind::Critical:
        icon_line(painter, cx, cy, size, 12, 3, 21, 12, color);
        icon_line(painter, cx, cy, size, 21, 12, 12, 21, color);
        icon_line(painter, cx, cy, size, 12, 21, 3, 12, color);
        icon_line(painter, cx, cy, size, 3, 12, 12, 3, color);
        icon_line(painter, cx, cy, size, 8, 8, 16, 16, color);
        icon_line(painter, cx, cy, size, 16, 8, 8, 16, color);
        break;
    case IconKind::Reset:
        icon_circle(painter, cx, cy, size, 12, 12, 7, color);
        icon_line(painter, cx, cy, size, 5, 12, 5, 7, color);
        icon_line(painter, cx, cy, size, 5, 7, 10, 7, color);
        icon_line(painter, cx, cy, size, 10, 7, 7, 4, color);
        icon_line(painter, cx, cy, size, 10, 7, 7, 10, color);
        break;
    }
}

Point bezier(Point a, Point b, Point c, float t) {
    auto inv = 1.0f - t;
    return {
        inv * inv * a.x + 2.0f * inv * t * b.x + t * t * c.x,
        inv * inv * a.y + 2.0f * inv * t * b.y + t * t * c.y,
    };
}

void draw_arc(phenotype::Painter& painter,
              std::array<Point, 3> const& points,
              Color color,
              float thickness) {
    Point prev = points[0];
    for (int i = 1; i <= 34; ++i) {
        auto t = static_cast<float>(i) / 34.0f;
        auto next = bezier(points[0], points[1], points[2], t);
        painter.line(prev.x, prev.y, next.x, next.y, thickness, color);
        prev = next;
    }
}

void draw_aircraft(phenotype::Painter& painter,
                   Point p,
                   Color color,
                   float scale = 1.0f) {
    auto nose = 12.0f * scale;
    auto wing = 7.0f * scale;
    painter.line(p.x - nose, p.y + 2.0f * scale, p.x + nose, p.y, 2.2f, color);
    painter.line(p.x - 2.0f * scale, p.y, p.x - wing, p.y - wing, 2.0f, color);
    painter.line(p.x - 2.0f * scale, p.y + 1.0f, p.x - wing, p.y + wing, 2.0f, color);
    painter.line(p.x - nose, p.y + 2.0f * scale, p.x - nose - 5.0f * scale, p.y - 3.0f * scale, 1.6f, color);
}

template<typename M>
void clickable_cell(std::string label,
                    float width,
                    float height,
                    Color bg,
                    Color fg,
                    Color border,
                    M msg,
                    bool focusable = false,
                    float font_size = 14.0f,
                    phenotype::TextAlign align = phenotype::TextAlign::Start) {
    using namespace phenotype;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Button);

    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.text = std::move(label);
    node.font_size = font_size;
    node.background = bg;
    node.text_color = fg;
    node.border_color = border;
    node.border_width = 0.75f;
    node.border_radius = 2.0f;
    node.style.padding[0] = 0.0f;
    node.style.padding[1] = 7.0f;
    node.style.padding[2] = 0.0f;
    node.style.padding[3] = 7.0f;
    node.style.max_width = width;
    node.style.fixed_height = height;
    node.style.text_align = align;
    node.hover_background = mix(bg, rgba(229, 231, 235), 0.55f);
    node.cursor_type = 1;
    node.callback_id = id;
    node.focusable = focusable;
    node.interaction_role = InteractionRole::Button;
    detail::attach_to_scope(h);
}

void label_cell(std::string label,
                float width,
                float height,
                Color bg,
                Color fg,
                Color border,
                float font_size = 14.0f,
                phenotype::TextAlign align = phenotype::TextAlign::Start) {
    using namespace phenotype;
    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.text = std::move(label);
    node.font_size = font_size;
    node.background = bg;
    node.text_color = fg;
    node.border_color = border;
    node.border_width = 0.75f;
    node.border_radius = 2.0f;
    node.style.padding[0] = 0.0f;
    node.style.padding[1] = 7.0f;
    node.style.padding[2] = 0.0f;
    node.style.padding[3] = 7.0f;
    node.style.max_width = width;
    node.style.fixed_height = height;
    node.style.text_align = align;
    detail::attach_to_scope(h);
}

template<typename M, typename Draw>
void clickable_canvas_cell(float width,
                           float height,
                           Color bg,
                           Color border,
                           M msg,
                           Draw draw,
                           bool focusable = false) {
    using namespace phenotype;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([msg = std::move(msg)] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Button);

    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.background = bg;
    node.border_color = border;
    node.border_width = 0.75f;
    node.border_radius = 2.0f;
    node.style.padding[0] = 0.0f;
    node.style.padding[1] = 0.0f;
    node.style.padding[2] = 0.0f;
    node.style.padding[3] = 0.0f;
    node.style.max_width = width;
    node.style.fixed_height = height;
    node.callback_id = id;
    node.cursor_type = 1;
    node.focusable = focusable;
    node.interaction_role = InteractionRole::Button;

    detail::open_container(h, [=] {
        widget::canvas(width, height, [=](Painter& painter) {
            draw(painter);
        });
    });
}

void draw_status_badge(phenotype::Painter& painter,
                       FlightStatus status,
                       float width,
                       float height,
                       Color border) {
    auto color = status_color(status);
    fill_rect(painter, 0.0f, 0.0f, width, height, status_bg(status));
    stroke_rect(painter, 0.0f, 0.0f, width - 1.0f, height - 1.0f, border);
    draw_icon(painter, status_icon_kind(status), 17.0f, height * 0.5f, 16.0f, color);
    draw_text_view(painter, 31.0f, height * 0.5f - 7.0f, status_label(status), 12.0f, color);
}

void status_badge_cell(FlightStatus status,
                       float width,
                       float height,
                       Color border) {
    using namespace phenotype;
    widget::canvas(width, height, [=](Painter& painter) {
        draw_status_badge(painter, status, width, height, border);
    });
}

void clickable_status_badge_cell(FlightStatus status,
                                 bool selected,
                                 SelectFlight msg) {
    auto border = selected ? rgba(37, 99, 235) : rgba(226, 232, 240);
    clickable_canvas_cell(88.0f, 38.0f, status_bg(status), border, msg, [=](phenotype::Painter& painter) {
        draw_status_badge(painter, status, 88.0f, 38.0f, border);
    });
}

void icon_cell(IconKind icon,
               float width,
               float height,
               Color bg,
               Color fg,
               Color border) {
    using namespace phenotype;
    widget::canvas(width, height, [=](Painter& painter) {
        fill_rect(painter, 0.0f, 0.0f, width, height, bg);
        stroke_rect(painter, 0.0f, 0.0f, width - 1.0f, height - 1.0f, border);
        draw_icon(painter, icon, width * 0.5f, height * 0.5f, 18.0f, fg);
    });
}

void icon_button(std::string label,
                 FlightAction action,
                 bool primary,
                 float width) {
    auto bg = primary ? rgba(37, 99, 235) : rgba(255, 255, 255);
    auto fg = primary ? rgba(255, 255, 255) : rgba(15, 23, 42);
    auto border = primary ? rgba(37, 99, 235) : rgba(226, 232, 240);
    auto icon = action_icon_kind(action);
    clickable_canvas_cell(width, 38.0f, bg, border, ApplyFlightAction{action},
        [=](phenotype::Painter& painter) {
            fill_rect(painter, 0.0f, 0.0f, width, 38.0f, bg);
            stroke_rect(painter, 0.0f, 0.0f, width - 1.0f, 37.0f, border);
            draw_icon(painter, icon, 15.0f, 19.0f, 15.0f, fg);
            draw_text(painter, 29.0f, 10.5f, label, 13.0f, fg);
        });
}

void metric_tile(IconKind icon,
                 std::string label,
                 std::string value,
                 Color accent,
                 float width = 176.0f) {
    using namespace phenotype;
    widget::canvas(width, 58.0f, [=](Painter& painter) {
        fill_rect(painter, 0.0f, 0.0f, width, 58.0f, rgba(255, 255, 255));
        fill_rect(painter, 0.0f, 0.0f, 5.0f, 58.0f, accent);
        stroke_rect(painter, 0.0f, 0.0f, width - 1.0f, 57.0f, rgba(226, 232, 240));
        fill_circle(painter, 28.0f, 29.0f, 14.0f, mix(accent, rgba(255, 255, 255), 0.82f));
        draw_icon(painter, icon, 28.0f, 29.0f, 20.0f, accent);
        draw_text(painter, 50.0f, 10.0f, label, 12.0f, rgba(100, 116, 139));
        draw_text(painter, 50.0f, 28.0f, value, 17.0f, rgba(15, 23, 42));
    });
}

void render_top_status_bar(State const& state) {
    using namespace phenotype;

    layout::card([&] {
        layout::grid({190.0f, 164.0f, 164.0f, 164.0f, 210.0f, 220.0f}, 58.0f, [&] {
            metric_tile(IconKind::Airport, "Airport", "ICN Ops", rgba(37, 99, 235), 190.0f);
            metric_tile(IconKind::Clock, "Local time", local_time_short(), rgba(20, 184, 166), 164.0f);
            metric_tile(IconKind::Flight, "Active flights", std::to_string(active_flight_count(state)), rgba(99, 102, 241), 164.0f);
            metric_tile(IconKind::Delay, "Delayed", std::to_string(delayed_flight_count(state)), rgba(245, 158, 11), 164.0f);
            metric_tile(IconKind::Weather, "Weather", state.weather, rgba(14, 165, 233), 210.0f);
            metric_tile(IconKind::Ops, "Ops mode", state.ops_mode, rgba(220, 38, 38), 220.0f);
        }, 8.0f);
    });
}

void render_flight_board(State const& state) {
    using namespace phenotype;
    auto header_bg = rgba(241, 245, 249);
    auto border = rgba(226, 232, 240);
    auto muted = rgba(71, 85, 105);

    layout::card([&] {
        layout::row([&] {
            widget::text("Flight board", TextSize::Heading);
            widget::text("select any row to retarget route, media, and timeline", TextSize::Small, TextColor::Muted);
        }, SpaceToken::Sm, CrossAxisAlignment::End, MainAxisAlignment::SpaceBetween);
        layout::spacer(10);
        layout::grid({78.0f, 88.0f, 58.0f, 72.0f, 88.0f, 84.0f, 70.0f}, 34.0f, [&] {
            label_cell("Flight", 78.0f, 34.0f, header_bg, muted, border, 12.0f);
            label_cell("Route", 88.0f, 34.0f, header_bg, muted, border, 12.0f);
            label_cell("Gate", 58.0f, 34.0f, header_bg, muted, border, 12.0f);
            label_cell("Type", 72.0f, 34.0f, header_bg, muted, border, 12.0f);
            label_cell("Status", 88.0f, 34.0f, header_bg, muted, border, 12.0f);
            label_cell("ETA/ETD", 84.0f, 34.0f, header_bg, muted, border, 12.0f);
            label_cell("Delay", 70.0f, 34.0f, header_bg, muted, border, 12.0f, TextAlign::End);

            for (auto const& flight : state.flights) {
                bool selected = flight.id == state.selected_id;
                auto row_bg = selected ? rgba(239, 246, 255) : rgba(255, 255, 255);
                auto row_fg = selected ? rgba(30, 64, 175) : rgba(15, 23, 42);
                auto row_border = selected ? rgba(37, 99, 235) : border;
                auto msg = SelectFlight{flight.id};
                clickable_cell(flight.number, 78.0f, 38.0f, row_bg, row_fg, row_border, msg);
                clickable_cell(route_text(flight), 88.0f, 38.0f, row_bg, row_fg, row_border, msg);
                clickable_cell(flight.gate, 58.0f, 38.0f, row_bg, row_fg, row_border, msg);
                clickable_cell(flight.aircraft, 72.0f, 38.0f, row_bg, row_fg, row_border, msg);
                clickable_status_badge_cell(flight.status, selected, msg);
                clickable_cell(flight.eta_etd, 84.0f, 38.0f, row_bg, row_fg, row_border, msg, false, 12.4f);
                clickable_cell(delay_text(flight), 70.0f, 38.0f, row_bg, row_fg, row_border, msg, false, 13.0f, TextAlign::End);
            }
        }, 0.0f);
    });
}

void render_route_canvas(State const& state) {
    using namespace phenotype;
    auto const& selected = selected_flight(state);

    layout::card([&] {
        layout::row([&] {
            widget::text("Route canvas", TextSize::Heading);
            widget::text(std::string("rev ") + std::to_string(state.revision), TextSize::Small, TextColor::Muted);
        }, SpaceToken::Sm, CrossAxisAlignment::End, MainAxisAlignment::SpaceBetween);
        layout::spacer(8);
        widget::canvas(kRouteCanvasWidth, kRouteCanvasHeight, [=, flights = state.flights](Painter& painter) {
            auto bg_top = rgba(248, 250, 252);
            auto bg_bottom = rgba(219, 234, 254);
            fill_gradient(painter, 0.0f, 0.0f, kRouteCanvasWidth, kRouteCanvasHeight, bg_top, bg_bottom, 20);
            for (int x = 30; x < static_cast<int>(kRouteCanvasWidth); x += 44)
                painter.line(static_cast<float>(x), 0.0f, static_cast<float>(x), kRouteCanvasHeight, 0.6f, rgba(203, 213, 225, 125));
            for (int y = 28; y < static_cast<int>(kRouteCanvasHeight); y += 38)
                painter.line(0.0f, static_cast<float>(y), kRouteCanvasWidth, static_cast<float>(y), 0.6f, rgba(203, 213, 225, 125));

            fill_rect(painter, 0.0f, 188.0f, kRouteCanvasWidth, 56.0f, rgba(226, 232, 240, 170));
            painter.line(12.0f, 208.0f, 126.0f, 226.0f, 1.0f, rgba(148, 163, 184));
            painter.line(210.0f, 214.0f, 370.0f, 202.0f, 1.0f, rgba(148, 163, 184));

            for (auto const& flight : flights) {
                bool active = flight.id == selected.id;
                auto color = active ? status_color(flight.status) : rgba(148, 163, 184, 155);
                draw_arc(painter, flight.route_points, color, active ? 3.0f : 1.1f);
            }

            auto pulse = static_cast<float>(state.animation_tick % 6) / 6.0f;
            auto marker_progress = std::clamp(selected.progress + pulse * 0.05f, 0.0f, 1.0f);
            if (selected.status == FlightStatus::Delayed || selected.status == FlightStatus::Maintenance)
                marker_progress = std::min(marker_progress, 0.34f);
            if (selected.status == FlightStatus::Landed)
                marker_progress = 1.0f;

            auto marker = bezier(selected.route_points[0], selected.route_points[1], selected.route_points[2], marker_progress);
            auto selected_color = status_color(selected.status);
            fill_circle(painter, selected.route_points[0].x, selected.route_points[0].y, 7.0f, rgba(255, 255, 255));
            stroke_circle(painter, selected.route_points[0].x, selected.route_points[0].y, 8.0f, selected_color, 2.0f);
            fill_circle(painter, selected.route_points[2].x, selected.route_points[2].y, 7.0f, rgba(255, 255, 255));
            stroke_circle(painter, selected.route_points[2].x, selected.route_points[2].y, 8.0f, selected_color, 2.0f);
            stroke_circle(painter, marker.x, marker.y, 16.0f + pulse * 10.0f, rgba(selected_color.r, selected_color.g, selected_color.b, 90));
            draw_aircraft(painter, marker, selected_color, 1.05f);

            draw_text(painter, 18.0f, 14.0f, selected.origin, 15.0f, rgba(15, 23, 42));
            draw_text(painter, kRouteCanvasWidth - 54.0f, 14.0f, selected.destination, 15.0f, rgba(15, 23, 42));
            draw_text(painter, 18.0f, kRouteCanvasHeight - 32.0f,
                std::string(selected.number) + " " + route_text(selected), 14.0f, rgba(15, 23, 42));
            draw_icon(painter, status_icon_kind(selected.status), 224.0f, kRouteCanvasHeight - 22.0f, 16.0f, selected_color);
            draw_text(painter, 240.0f, kRouteCanvasHeight - 32.0f,
                std::string(status_label(selected.status)) + " pulse " + std::to_string(state.animation_tick % 10),
                13.0f, selected_color);
        });
    });
}

void render_destination_media(State const& state) {
    using namespace phenotype;
    auto const& flight = selected_flight(state);
    auto visual = flight.visual;

    layout::card([&] {
        widget::text("Destination", TextSize::Heading);
        layout::spacer(8);
        widget::canvas(kMediaCanvasWidth, kMediaCanvasHeight, [=](Painter& painter) {
            fill_gradient(painter, 0.0f, 0.0f, kMediaCanvasWidth, kMediaCanvasHeight,
                          visual.sky_top, visual.sky_bottom, 18);
            fill_circle(painter, 238.0f, 36.0f, 21.0f, mix(visual.accent, rgba(255, 255, 255), 0.25f));
            fill_circle(painter, 72.0f, 44.0f, 12.0f, rgba(255, 255, 255, 210));
            fill_circle(painter, 90.0f, 42.0f, 17.0f, rgba(255, 255, 255, 210));
            fill_circle(painter, 111.0f, 49.0f, 11.0f, rgba(255, 255, 255, 210));
            fill_rect(painter, 0.0f, 112.0f, kMediaCanvasWidth, 42.0f, rgba(15, 23, 42, 190));
            for (int i = 0; i < 9; ++i) {
                auto x = 16.0f + static_cast<float>(i) * 28.0f;
                auto h = 20.0f + static_cast<float>((i * 11) % 34);
                fill_rect(painter, x, 112.0f - h, 16.0f, h, rgba(30, 41, 59, 180));
                painter.line(x + 4.0f, 112.0f - h + 9.0f, x + 12.0f, 112.0f - h + 9.0f, 1.0f, rgba(226, 232, 240, 180));
            }
            draw_text(painter, 16.0f, 120.0f, std::string(flight.destination) + " / " + visual.city, 16.0f, rgba(255, 255, 255));
            draw_icon(painter, IconKind::Weather, 207.0f, 132.0f, 15.0f, rgba(255, 255, 255));
            draw_text(painter, 220.0f, 121.0f, visual.weather, 13.0f, rgba(255, 255, 255));
        });
        layout::spacer(10);
        layout::grid({88.0f, 82.0f, 120.0f}, 34.0f, [&] {
            label_cell(std::string("WX ") + flight.visual.weather, 88.0f, 34.0f, status_bg(flight.status), status_color(flight.status), rgba(226, 232, 240), 13.0f);
            label_cell(flight.visual.temp, 82.0f, 34.0f, rgba(255, 255, 255), rgba(15, 23, 42), rgba(226, 232, 240), 13.0f);
            label_cell(flight.gate, 120.0f, 34.0f, rgba(255, 255, 255), rgba(15, 23, 42), rgba(226, 232, 240), 13.0f);
        }, 5.0f);
        layout::spacer(8);
        widget::text(std::string(flight.visual.terminal_note), TextSize::Small, TextColor::Muted);
    });
}

void render_timeline(State const& state, Flight const& flight) {
    using namespace phenotype;
    struct Stage {
        char const* label;
        float at;
    };
    constexpr std::array<Stage, 6> stages = {{
        {"Check-in", 0.08f},
        {"Board", 0.30f},
        {"Push", 0.44f},
        {"Taxi", 0.56f},
        {"Takeoff", 0.68f},
        {"Landing", 1.0f},
    }};

    widget::canvas(kTimelineWidth, kTimelineHeight, [=](Painter& painter) {
        fill_rect(painter, 0.0f, 0.0f, kTimelineWidth, kTimelineHeight, rgba(248, 250, 252));
        auto base_y = 28.0f;
        auto left = 20.0f;
        auto right = kTimelineWidth - 22.0f;
        auto progress_x = left + (right - left) * std::clamp(flight.progress, 0.0f, 1.0f);
        painter.line(left, base_y, right, base_y, 7.0f, rgba(226, 232, 240));
        painter.line(left, base_y, progress_x, base_y, 7.0f, status_color(flight.status));
        fill_circle(painter, progress_x, base_y, 6.0f + static_cast<float>(state.animation_tick % 4), rgba(status_color(flight.status).r, status_color(flight.status).g, status_color(flight.status).b, 95));
        for (auto const& stage : stages) {
            auto x = left + (right - left) * stage.at;
            auto done = flight.progress + 0.02f >= stage.at;
            fill_circle(painter, x, base_y, done ? 7.0f : 5.0f,
                        done ? status_color(flight.status) : rgba(203, 213, 225));
            draw_text(painter, x - 24.0f, 49.0f, stage.label, 11.0f,
                      done ? rgba(15, 23, 42) : rgba(100, 116, 139));
        }
        draw_text(painter, 16.0f, 7.0f, std::string("timeline ") + percent_text(flight.progress), 12.0f, rgba(71, 85, 105));
        draw_icon(painter, status_icon_kind(flight.status), 252.0f, 16.5f, 14.0f, status_color(flight.status));
        draw_text_view(painter, 266.0f, 7.0f, status_label(flight.status), 12.0f, status_color(flight.status));
    });
}

void render_detail_panel(State const& state) {
    using namespace phenotype;
    auto const& flight = selected_flight(state);

    layout::card([&] {
        layout::row([&] {
            layout::column([&] {
                widget::text(std::string(flight.number) + " " + route_text(flight), TextSize::Heading);
                widget::text(flight_summary(flight), TextSize::Small, TextColor::Muted);
            }, SpaceToken::Xs);
            status_badge_cell(flight.status, 110.0f, 32.0f, status_color(flight.status));
        }, SpaceToken::Sm, CrossAxisAlignment::Center, MainAxisAlignment::SpaceBetween);
        layout::spacer(10);
        layout::grid({104.0f, 102.0f, 104.0f}, 42.0f, [&] {
            label_cell(std::string("Gate ") + flight.gate, 104.0f, 42.0f, rgba(255, 255, 255), rgba(15, 23, 42), rgba(226, 232, 240));
            label_cell(flight.aircraft, 102.0f, 42.0f, rgba(255, 255, 255), rgba(15, 23, 42), rgba(226, 232, 240));
            label_cell(flight.eta_etd, 104.0f, 42.0f, rgba(255, 255, 255), rgba(15, 23, 42), rgba(226, 232, 240));
            label_cell(flight.crew, 104.0f, 42.0f, rgba(248, 250, 252), rgba(71, 85, 105), rgba(226, 232, 240));
            label_cell(std::string("Bag ") + flight.baggage_belt, 102.0f, 42.0f, rgba(248, 250, 252), rgba(71, 85, 105), rgba(226, 232, 240));
            label_cell(flight.tail_number, 104.0f, 42.0f, rgba(248, 250, 252), rgba(71, 85, 105), rgba(226, 232, 240));
        }, 6.0f);
        layout::spacer(12);
        render_timeline(state, flight);
        layout::spacer(10);
        layout::grid({68.0f, 72.0f, 82.0f, 68.0f, 70.0f}, 38.0f, [&] {
            icon_button("Delay", FlightAction::Delay, false, 68.0f);
            icon_button("Board", FlightAction::Board, true, 72.0f);
            icon_button("Depart", FlightAction::Depart, true, 82.0f);
            icon_button("Land", FlightAction::Land, false, 68.0f);
            icon_button("Reset", FlightAction::Reset, false, 70.0f);
        }, 6.0f);
    });
}

void render_activity_feed(State const& state) {
    using namespace phenotype;
    layout::card([&] {
        widget::text("Activity feed", TextSize::Heading);
        layout::spacer(8);
        layout::grid({38.0f, 54.0f, 200.0f}, 42.0f, [&] {
            for (auto const& item : state.feed) {
                auto color = severity_color(item.severity);
                icon_cell(severity_icon_kind(item.severity), 38.0f, 42.0f, severity_bg(item.severity), color, color);
                label_cell(item.time, 54.0f, 42.0f, rgba(255, 255, 255), rgba(71, 85, 105), rgba(226, 232, 240), 12.0f);
                label_cell(trunc(item.title + " - " + item.detail, 36), 200.0f, 42.0f,
                           rgba(255, 255, 255), rgba(15, 23, 42), rgba(226, 232, 240), 12.0f);
            }
        }, 0.0f);
    });
}

void view(State const& state) {
    using namespace phenotype;

    layout::padded(SpaceToken::Lg, [&] {
        layout::column([&] {
            render_top_status_bar(state);
            layout::grid({580.0f, 414.0f, 332.0f}, 0.0f, [&] {
                layout::column([&] {
                    render_flight_board(state);
                    render_activity_feed(state);
                }, SpaceToken::Md);
                layout::column([&] {
                    render_route_canvas(state);
                    render_detail_panel(state);
                }, SpaceToken::Md);
                layout::column([&] {
                    render_destination_media(state);
                    layout::card([&] {
                        widget::text("Operations summary", TextSize::Heading);
                        layout::spacer(8);
                        widget::code(
                            std::string("selected: ") + flight_summary(selected_flight(state)) +
                            "\nactive_flights: " + std::to_string(active_flight_count(state)) +
                            "\ndelayed_count: " + std::to_string(delayed_flight_count(state)) +
                            "\nrevision: " + std::to_string(state.revision) +
                            "\nroute_pulse_tick: " + std::to_string(state.animation_tick));
                    });
                }, SpaceToken::Md);
            }, 12.0f);
        }, SpaceToken::Md);
    });
}

} // namespace

int main() {
    phenotype::set_theme({
        .background = {241, 245, 249, 255},
        .foreground = {15, 23, 42, 255},
        .accent = {37, 99, 235, 255},
        .accent_strong = {30, 64, 175, 255},
        .muted = {100, 116, 139, 255},
        .border = {226, 232, 240, 255},
        .surface = {255, 255, 255, 255},
        .code_bg = {248, 250, 252, 255},
        .hero_bg = {239, 246, 255, 255},
        .hero_fg = {15, 23, 42, 255},
        .hero_muted = {71, 85, 105, 255},
    });
    return phenotype::native::run_app<State, Msg>(
        1360,
        860,
        "Flight Operations Board",
        view,
        update);
}
