# ID6 native source

Start with [AgentReadMe.md](../AgentReadMe.md). See [signal definitions](docs/SIGNALS.md) and [porting](docs/PORTING.md).

For a reviewed request plan, offline sanitized reports and the staged diesel
vehicle checks, see [Diesel discovery and validation](docs/DIESEL-VALIDATION.md).

[ShiftDeck engine profiles](docs/SHIFT-PROFILES.md) covers diesel target/display
settings, legacy preferences, resource generation and host validation limits.

[Engine fault-code handling](docs/FAULT-CODES.md) describes supported status bits,
the DDE/raw view, separate standard confirmed/pending/permanent emissions-code
reads, description limitations and explicit clear confirmation. Standard OBD
codes are parsed from their own response format, never shortened BMW UDS codes.

BoostDeck's [diesel rail display scale](docs/BOOST-DIESEL-SCALE.md) is selectable
without changing the gasoline range or saved gauge configuration. Host tests do
not establish vehicle support.
[Automatic diesel gauges](docs/AUTO-DIESEL-GAUGES.md) uses explicit PID/EGS
evidence for new defaults while retaining existing custom layouts.

[Worker lifecycle regression coverage](docs/WORKER-LIFECYCLE.md) exercises the
existing PR #143 process-ownership fix with production plugins and host children.

WarmDeck's [independent temperature targets](docs/WARM-READINESS.md) retain fresh
fluid states when another reading is unavailable. Its app targets are advisory;
the display refers drivers to the vehicle's guidance instead of a generic RPM limit.

[Standard exhaust-temperature gauges](docs/STANDARD-EGT.md) use advertised PID
and sensor-support data, retain explicit bank/sensor labels, and preserve old
channel IDs and ordinary packets. Exact N57 support still needs vehicle evidence.
