package main

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"
	"testing/fstest"
)

func fullDpfFixture(t *testing.T) fstest.MapFS {
	t.Helper()
	data := dpfFixture(t)
	for _, name := range fullDpfApp.requiredFiles() {
		data["payload/"+fullDpfApp.name+"/"+name] = &fstest.MapFile{Data: []byte("host-only full DPF fixture: " + name)}
	}
	data["payload/ID6FullDPFDeck/ID6FullDPFDeck.manifest"] = &fstest.MapFile{Data: []byte(`<plugin><name>ID6FullDPFDeck</name><config><dependencies><pluginAppConnector><id>243</id></pluginAppConnector></dependencies></config></plugin>`)}
	data["payload/ID6LibraryLab/dpfdeck-collector-qnx"] = &fstest.MapFile{Data: []byte("host-only worker fixture; never execute")}
	return data
}

func TestFullDpfIsAnExplicitSelectionWithItsOwnPayloadGate(t *testing.T) {
	data := fullDpfFixture(t)
	delete(data, "payload/DPFDeck/libDPFDeck.so")
	if dpfPayloadAvailable() || !fullDpfPayloadAvailable() {
		t.Fatal("standard and full payload gates are coupled")
	}
	selected, err := chooseFullscreenExtras([]string{"DPFDeck"})
	if err != nil || len(selected) != 1 || selected[0] != dpfApp {
		t.Fatal(selected, err)
	}
	for _, app := range fullscreenApps {
		if app == fullDpfApp {
			t.Fatal("optional DPF entered default dashboards")
		}
	}
	if _, err = chooseFullscreenExtras([]string{"DPFDeck", "DPFDeck"}); err == nil {
		t.Fatal("duplicate selection accepted")
	}
	if _, err = chooseFullscreenExtras([]string{"Unknown"}); err == nil {
		t.Fatal("unknown menu accepted")
	}
	delete(data, "payload/ID6LibraryLab/dpfdeck-collector-qnx")
	if _, err = chooseFullscreenExtras([]string{"DPFDeck"}); err == nil {
		t.Fatal("missing shared worker accepted")
	}
}

func TestFullDpfRejectsWrongIdentityAndHostFixturePayload(t *testing.T) {
	data := fullDpfFixture(t)
	path := "payload/ID6FullDPFDeck/ID6FullDPFDeck.manifest"
	correct := data[path]
	data[path] = &fstest.MapFile{Data: bytes.ReplaceAll(correct.Data, []byte("243"), []byte("239"))}
	if fullDpfPayloadAvailable() {
		t.Fatal("Bootmod3 connector accepted for DPF")
	}
	data[path] = correct
	data["payload/TEST-ONLY-DPF-FIXTURE"] = &fstest.MapFile{Data: []byte("non-deployable fixture")}
	if fullDpfPayloadAvailable() {
		t.Fatal("host-only payload offered for installation")
	}
}

func TestFullDpfKeepsExistingConnectorsAndSupportsAllFullPanels(t *testing.T) {
	seen := map[string]string{}
	for _, app := range append(append(append([]plugin{}, fullscreenApps...), fullscreenCompanions...), fullDpfApp) {
		if old, ok := seen[app.connector]; ok {
			t.Fatal("duplicate connector", old, app.name, app.connector)
		}
		seen[app.connector] = app.name
	}
	if seen["239"] != "ID6FullBootmod3Suite" || seen["243"] != "ID6FullDPFDeck" {
		t.Fatal(seen)
	}
	for _, screen := range []panel{{800, 480}, {960, 480}, {1280, 480}, {1440, 540}, {1920, 720}} {
		if err := checkAppLayouts(screen, []plugin{fullDpfApp}); err != nil {
			t.Fatal(err)
		}
	}
}

func TestFullDpfPublishUninstallAndRollbackPreserveSettings(t *testing.T) {
	for _, failure := range []string{"", "readback", "publish"} {
		t.Run(failure, func(t *testing.T) {
			data := fullDpfFixture(t)
			dir := t.TempDir()
			folder := filepath.Join(dir, "Plugins", fullDpfApp.name)
			if err := os.MkdirAll(folder, 0755); err != nil {
				t.Fatal(err)
			}
			old := data["payload/ID6FullDPFDeck/ID6FullDPFDeck.manifest"].Data
			if err := os.WriteFile(filepath.Join(folder, fullDpfApp.name+".manifest"), old, 0644); err != nil {
				t.Fatal(err)
			}
			if err := os.WriteFile(filepath.Join(folder, "gateway-ip.txt"), []byte("169.254.20.4\n"), 0600); err != nil {
				t.Fatal(err)
			}
			standard := filepath.Join(dir, "Plugins", "DPFDeck")
			if err := os.MkdirAll(standard, 0755); err != nil {
				t.Fatal(err)
			}
			if err := os.WriteFile(filepath.Join(standard, "DPFDeck.manifest"), data["payload/DPFDeck/DPFDeck.manifest"].Data, 0644); err != nil {
				t.Fatal(err)
			}
			for _, name := range []string{"DPFDeck.prefs", "ID6LibraryLab.dpf.prefs"} {
				if err := os.WriteFile(filepath.Join(dir, name), []byte{'D', 2, 1, 0, 1}, 0600); err != nil {
					t.Fatal(err)
				}
			}
			tx, err := install(shellFixture{dir: dir, corruptDPFRead: failure == "readback", failDPFRename: failure == "publish"}, "169.254.21.3", panel{1280, 480}, false, fullDpfApp)
			if failure != "" {
				if err == nil {
					t.Fatal("failed upload was published")
				}
				value, e := os.ReadFile(filepath.Join(folder, "gateway-ip.txt"))
				if e != nil || string(value) != "169.254.20.4\n" {
					t.Fatal("rollback lost existing settings", e)
				}
				if _, e := os.Stat(filepath.Join(standard, "DPFDeck.manifest")); e != nil {
					t.Fatal("rollback lost standard DPF menu", e)
				}
			} else {
				if err != nil || tx == "" {
					t.Fatal(tx, err)
				}
				if _, e := os.Stat(standard); !os.IsNotExist(e) {
					t.Fatal("standard DPF menu was not backed up", e)
				}
				if _, e := os.Stat(filepath.Join(dir, filepath.Base(tx), "old-DPFDeck", "DPFDeck.manifest")); e != nil {
					t.Fatal("missing standard DPF backup", e)
				}
				if _, _, err = uninstall(shellFixture{dir: dir}, []plugin{fullDpfApp}); err != nil {
					t.Fatal(err)
				}
				if _, err = os.Stat(folder); !os.IsNotExist(err) {
					t.Fatal("full DPF folder remains", err)
				}
			}
			for _, name := range []string{"DPFDeck.prefs", "ID6LibraryLab.dpf.prefs"} {
				value, e := os.ReadFile(filepath.Join(dir, name))
				if e != nil || !bytes.Equal(value, []byte{'D', 2, 1, 0, 1}) {
					t.Fatal("preferences changed", name, e)
				}
			}
		})
	}
}
