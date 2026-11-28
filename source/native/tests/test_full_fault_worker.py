"""Apply the existing synthetic protocol/ownership oracles to the full-screen build."""
from pathlib import Path
import tempfile
import unittest

import test_drive_apps as apps
import test_obd_faults as obd
import test_oneshot_most as most

BUILD=Path(__file__).resolve().parents[1]/'build'


class FullFaultWorker(obd.ObdFaultsTest):
    def setUp(self):
        directory=tempfile.TemporaryDirectory(prefix='full-fault-worker-')
        self.addCleanup(directory.cleanup)
        binaries=Path(directory.name)
        (binaries/'engine-codes').symlink_to(BUILD/'full-engine-codes')
        for module in (apps,most):
            old=module.BUILD;self.addCleanup(setattr,module,'BUILD',old);module.BUILD=binaries

    def test_bmw_default_is_read_only_and_keeps_availability(self):
        case=apps.DriveAppsTest()
        case.test_fault_reads_default_to_read_only_and_preserve_supported_status()
        case.test_fault_status_unavailable_is_not_reported_as_no_codes()
        case.test_fault_metadata_rejects_incomplete_records_and_retains_leading_zero_codes()
        case.test_fault_read_refusal_and_lost_reply_never_attempt_clear_or_session_changes()
        case.test_clear_without_confirmation_never_opens_the_gateway()

    def test_bmw_pipe_and_most_cleanup_limits_are_shared(self):
        case=most.EngineCodesTest()
        case.test_engine_codes_metadata_fits_plugin_pipe_and_keeps_transport_limits()
        case.test_engine_codes_most_read()
        case.test_engine_codes_busy()


if __name__=='__main__': unittest.main()
