# ID6 native source

Start with [AgentReadMe.md](../AgentReadMe.md). See [signal definitions](docs/SIGNALS.md) and [porting](docs/PORTING.md).

For a reviewed request plan, offline sanitized reports and the staged diesel
vehicle checks, see [Diesel discovery and validation](docs/DIESEL-VALIDATION.md).

[ShiftDeck engine profiles](docs/SHIFT-PROFILES.md) covers diesel target/display
settings, legacy preferences, resource generation and host validation limits.

[Engine fault-code handling](docs/FAULT-CODES.md) describes supported status bits,
the DDE/raw view, description limitations and explicit clear confirmation.

BoostDeck's [diesel rail display scale](docs/BOOST-DIESEL-SCALE.md) is selectable
without changing the gasoline range or saved gauge configuration. Host tests do
not establish vehicle support.
[Automatic diesel gauges](docs/AUTO-DIESEL-GAUGES.md) uses explicit PID/EGS
evidence for new defaults while retaining existing custom layouts.
