"""Synthetic parser/math tests; these are not device measurements."""
import importlib.util
import tempfile
from pathlib import Path

module = Path(__file__).resolve().parents[2] / "tools/compare_power_windows.py"
spec = importlib.util.spec_from_file_location("windows", module)
windows = importlib.util.module_from_spec(spec)
spec.loader.exec_module(windows)

with tempfile.TemporaryDirectory() as directory:
    old, new = Path(directory) / "old.csv", Path(directory) / "new.csv"
    header = "elapsed_s,battery_voltage_v,battery_current_ma,usb_connected\n"
    old.write_text(header + "0,4,20,0\n600,4,20,0\n")
    new.write_text(header + "0,4,10,0\n600,4,10,0\n")
    a, b = windows.read_window(old), windows.read_window(new)
    result = windows.compare(a, b)
    assert result["conditional_runtime_multiplier"] == 2
    assert result["measured_extra_hours"] is None
    assert abs(a["energy_mwh"] - 13.3333333333) < 1e-8
    new.write_text(header + "0,4,,0\n600,3.99,,0\n")
    assert "conditional_runtime_multiplier" not in windows.compare(a, windows.read_window(new))
    new.write_text(header + "0,4,10,1\n600,4,10,1\n")
    try: windows.read_window(new)
    except ValueError: pass
    else: raise AssertionError("USB window accepted")
    new.write_text(header + "0,4,10,0\n500,4,10,0\n")
    try: windows.compare(a, windows.read_window(new))
    except ValueError: pass
    else: raise AssertionError("Different-duration windows accepted")
print("PASS: synthetic equal-time analysis, current/energy integration, USB/duration rejection, no hours from voltage")
