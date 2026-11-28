"""Run canonical DPF transport and coexistence oracles against the full-screen build."""
from pathlib import Path
import tempfile
import unittest
import test_dpf_transport as transport
import test_dpf_coexistence as coexistence

BUILD=Path(__file__).resolve().parents[1]/'build'

class FullDpfTransport(transport.DpfTransportTest):
    def setUp(self):
        previous=transport.BIN;self.addCleanup(setattr,transport,'BIN',previous)
        transport.BIN=BUILD/'full-dpf-deck'

class FullDpfCoexistence(coexistence.DpfCoexistenceTest):
    def setUp(self):
        directory=tempfile.TemporaryDirectory(prefix='full-dpf-coexistence-');self.addCleanup(directory.cleanup)
        root=Path(directory.name)
        for name,target in (('dpfdeck-collector','full-dpf-deck'),('boost-deck','full-boost-deck')):
            (root/name).symlink_to(BUILD/target)
        previous=coexistence.BUILD;self.addCleanup(setattr,coexistence,'BUILD',previous)
        coexistence.BUILD=root

if __name__=='__main__': unittest.main()
