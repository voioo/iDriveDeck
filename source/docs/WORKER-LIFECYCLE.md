# Worker ownership when child status is unavailable

The original plugins cleared a worker PID on `waitpid(..., WNOHANG)` returning
`ECHILD`. A controller could then start another worker after its retry deadline
and lose the PID it needed to stop the original process. This is a lifecycle
failure mode, not evidence of the cause of any particular F15/F16 freeze.

This branch reuses Andreas Böhm's existing MIT-licensed fix from
[PR #143](https://github.com/inotbarnacle/iDriveDeck/pull/143), commit
[`d63bd1f`](https://github.com/inotbarnacle/iDriveDeck/commit/d63bd1f1036b78a41917ef1d87f809c337de3287).
Its author and cherry-pick reference are preserved. The process is forgotten
after a successful reap, or after `ECHILD` followed by `kill(pid, 0)` reporting
`ESRCH`. Permission errors and uncertain failures retain the PID. BoostDeck's
tone process and the HUD menu are included along with the other reapers.

QNX 6.5 documents that a parent using `SIGCHLD = SIG_IGN` cannot wait on child
deaths. Its `ECHILD` result does not supply a process exit status.
[QNX waitpid reference](https://www.qnx.com/developers/docs/6.5.0SP1.update/com.qnx.doc.neutrino_lib_ref/w/waitpid.html)
Signal zero checks process validity without delivering a signal; `ESRCH` means
the PID is absent, while `EPERM` concerns permission.
[QNX kill reference](https://qnx.com/developers/docs/6.5.0SP1.update/com.qnx.doc.neutrino_lib_ref/k/kill.html)
The live-process interpretation used here is the existing contributor's reported
HMI scenario. Host tests simulate that return path; they do not prove that Linux
and the actual head unit return identical statuses in every circumstance.

## Regression coverage

```sh
make -C source/native test-lifecycle
make -C source/native test
```

The test manifest covers all 13 current plugin files that call `waitpid`, with
14 process paths because BoostDeck owns a collector and a tone process. Each
executable includes its actual production plugin and the MIT BMW framework fake
previously supplied in PR #143. No proprietary framework is linked.

The harness exercises running, interrupted, `ECHILD`/alive, `ECHILD`/permission
denied, other uncertain probe errors, `ECHILD`/gone and successfully reaped
states. Signal-zero probes and stop behavior also run against an inert, owned
host child: a simulated `ECHILD` must retain that live child, closing must send
it SIGTERM, and confirmed termination must release its PID. Real signals are
restricted to that test-owned child; the child has a parent monitor and bounded
lifetime for failure cleanup.

BoostDeck, ShiftDeck, WarmDeck and EngineCodes additionally exercise their
production controller paths across ten simulated retry deadlines. A live
stand-in must remain the only launched worker, and closing must still reach its
original PID. All diagnostic launches are intercepted, and clear requests are
rejected by the harness. No actual vehicle worker or drivetrain command runs.

The coverage script enumerates direct `waitpid` users and requires a test
adapter for each. This replaces the original patch's Makefile line-count guard;
the copied production fix itself is unchanged. CI also compiles the same harness
against a separate, pinned upstream `8567aba` checkout. Every old plugin must
compile successfully and then fail specifically for forgetting a live PID.
Compilation failures or unrelated failures do not count as evidence of the bug.
The four dashboard/read controllers are also run in isolation against that
baseline and must reproduce a duplicate launch after the retry deadline.

To repeat that comparison locally, provide a separate checkout at the audited
baseline after building the test objects:

```sh
python3 source/tools/check_worker_lifecycle.py --baseline-root /path/to/baseline-checkout
```

CI runs GCC/Clang host regressions, C++98 production syntax, Clang analysis and
ASan/UBSan for every reaper.
Host syntax checks suppress the pre-existing SDK callback-overload hiding warning
in menus such as BEAMer; other enabled warnings remain errors. Callback names
are left intact by this ownership fix.
The local Clang stream checker also reports two inherited findings in
`launch_records.h:39` (reading after a failed/EOF read). Both occur in the pinned
baseline as well. They remain a separate file-I/O follow-up; this analysis is
not described as warning-free.
The source fix belongs to the existing PR #143; the regression additions are a separate commit so its contributor or maintainer
can reuse them independently. The fork integration branch can test this
dependency alongside the diesel contributions while upstream review proceeds.

## Limits

No diagnostic definition, ECU request, display asset, firmware approval or
vehicle configuration changes in this fix. It preserves ownership when status
is unavailable; it does not establish a trusted process identity across arbitrary
PID reuse, guarantee successful signal delivery under changed permissions, or
prove factory-function compatibility. Hidden polling/timer changes and other
lifecycle work in PR #143 remain separate.

This is host validation with simulated wait outcomes and real host child cleanup,
not a QNX build, head-unit runtime test, diagnosis of the owner's X5, standalone
transport validation or authorization to install/restart anything. Physical
cleanup and factory-function checks still belong to the staged vehicle milestone.
