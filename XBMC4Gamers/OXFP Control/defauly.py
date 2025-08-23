# -*- coding: utf-8 -*-
# OXFP Controller for XBMC4Gamers / XBMC4Xbox (Python 2.7-safe)
from __future__ import print_function
import sys, os, socket, time

# ---------------- XBMC bindings ----------------
try:
    import xbmc
    import xbmcgui
except Exception:
    xbmc = None
    xbmcgui = None

# ---------------- JSON compat ------------------
try:
    import json as _json
    _json_available = True
except:
    try:
        import simplejson as _json
        _json_available = True
    except:
        _json_available = False

try:
    unicode
except NameError:
    unicode = str  # if someone runs this on py3 desktop for testing

def _mini_json_dumps(obj):
    if obj is None: return "null"
    if obj is True: return "true"
    if obj is False: return "false"
    t = type(obj)
    if t in (int, long, float): return str(obj)
    if t in (str, unicode):
        s = obj.replace('\\', '\\\\').replace('"', '\\"')
        return '"' + s + '"'
    if isinstance(obj, (list, tuple)):
        return "[" + ",".join([_mini_json_dumps(x) for x in obj]) + "]"
    if isinstance(obj, dict):
        parts, keys = [], obj.keys()
        try: keys.sort()
        except: pass
        for k in keys:
            parts.append(_mini_json_dumps(k) + ":" + _mini_json_dumps(obj[k]))
        return "{" + ",".join(parts) + "}"
    return _mini_json_dumps(str(obj))

def json_dumps(obj):
    if _json_available:
        try: return _json.dumps(obj, separators=(',', ':'))
        except: pass
    return _mini_json_dumps(obj)

def json_loads(s):
    if _json_available:
        try: return _json.loads(s)
        except: pass
    try:
        s2 = s.replace("true","True").replace("false","False").replace("null","None")
        return eval(s2, {"__builtins__":{}})
    except:
        return None

# ---------------- Settings ---------------------
CFG_PATH = os.path.join(os.path.dirname(__file__), "oxfp_udp.cfg")

def load_cfg():
    cfg = {"ip":"", "port":32123, "timeout_ms":300, "http_port":80}
    try:
        if os.path.exists(CFG_PATH):
            txt = open(CFG_PATH, "rb").read()
            obj = json_loads(txt)
            if isinstance(obj, dict): cfg.update(obj)
    except: pass
    return cfg

def save_cfg(cfg):
    try: open(CFG_PATH, "wb").write(json_dumps(cfg))
    except: pass

# ---------------- UDP client -------------------
class OXFPClient(object):
    def __init__(self, ip, port, timeout_ms):
        self.ip = ip
        self.port = int(port)
        self.timeout = max(80, int(timeout_ms)) / 1000.0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try: self.sock.settimeout(self.timeout)
        except: pass

    def _send(self, payload_dict, broadcast=False):
        data = json_dumps(payload_dict)
        addr = (self.ip, self.port)
        if broadcast:
            try: self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            except: pass
            addr = ("255.255.255.255", self.port)
        try:
            self.sock.sendto(data, addr)
            resp, _ = self.sock.recvfrom(4096)
            return json_loads(resp)
        except socket.timeout:
            return None
        except Exception as e:
            return {"ok": False, "err": "sock_error: %s" % str(e)}

    def ping(self, broadcast=False, seq=1): return self._send({"op":"ping","seq":seq}, broadcast=broadcast)
    def get(self, seq=1): return self._send({"op":"get","seq":seq})
    def preview(self, fields, seq=1, previewMs=None):
        pkt = {"op":"preview","seq":seq}; pkt.update(fields)
        if previewMs: pkt["previewMs"] = int(previewMs)
        return self._send(pkt)
    def set_live(self, fields, seq=1):
        pkt = {"op":"set","seq":seq}; pkt.update(fields)
        return self._send(pkt)
    def save(self, seq=1): return self._send({"op":"save","seq":seq})
    def reset(self, seq=1): return self._send({"op":"reset","seq":seq})
    def identify(self, ms=1500, seq=1): return self._send({"op":"identify","seq":seq,"ms":int(ms)})
    def mode(self, mode_id, seq=1): return self._send({"op":"mode","seq":seq,"mode":int(mode_id)})

# --------------- HTTP fallback -----------------
try:
    import urllib2
except:
    urllib2 = None

def http_get(ip, port, path, timeout_ms):
    if not urllib2: return None
    url = "http://%s:%d%s" % (ip, int(port), path)
    try:
        return urllib2.urlopen(url, timeout=max(100, timeout_ms)/1000.0).read()
    except:
        return None

def http_post_json(ip, port, path, obj, timeout_ms):
    if not urllib2: return None
    url = "http://%s:%d%s" % (ip, int(port), path)
    data = json_dumps(obj)
    try:
        req = urllib2.Request(url, data, {"Content-Type":"application/json"})
        f = urllib2.urlopen(req, timeout=max(100, timeout_ms)/1000.0)
        return f.read()
    except:
        return None

def http_get_config(ip, http_port, timeout_ms):
    txt = http_get(ip, http_port, "/api/ledconfig", timeout_ms)
    if not txt: return None
    return json_loads(txt)

def http_merge_and_preview(ip, http_port, timeout_ms, partial_fields):
    cur = http_get_config(ip, http_port, timeout_ms)
    if not cur: return None
    body = {
        "mode":       cur.get("mode", 0),
        "brightness": cur.get("brightness", 128),
        "greenColor": cur.get("greenColor", [0,255,0]),
        "redColor":   cur.get("redColor", [255,0,0]),
        "orangeColor":cur.get("orangeColor", [255,128,0]),
        "animMode":   cur.get("animMode", 0),
        "animColorA": cur.get("animColorA", [0,128,255]),
        "animColorB": cur.get("animColorB", [255,0,128]),
        "animSpeed":  cur.get("animSpeed", 5)
    }
    body.update(partial_fields)
    return http_post_json(ip, http_port, "/api/ledpreview", body, timeout_ms)

def http_merge_and_save(ip, http_port, timeout_ms, partial_fields=None):
    cur = http_get_config(ip, http_port, timeout_ms)
    if not cur: return None
    body = {
        "mode":       cur.get("mode", 0),
        "brightness": cur.get("brightness", 128),
        "greenColor": cur.get("greenColor", [0,255,0]),
        "redColor":   cur.get("redColor", [255,0,0]),
        "orangeColor":cur.get("orangeColor", [255,128,0]),
        "animMode":   cur.get("animMode", 0),
        "animColorA": cur.get("animColorA", [0,128,255]),
        "animColorB": cur.get("animColorB", [255,0,128]),
        "animSpeed":  cur.get("animSpeed", 5)
    }
    if partial_fields: body.update(partial_fields)
    return http_post_json(ip, http_port, "/api/ledsave", body, timeout_ms)

def http_reset(ip, http_port, timeout_ms):
    if not urllib2: return None
    url = "http://%s:%d/api/ledreset" % (ip, int(http_port))
    try:
        req = urllib2.Request(url, "")
        f = urllib2.urlopen(req, timeout=max(100, timeout_ms)/1000.0)
        return f.read()
    except:
        return None

# --------------- Discovery ---------------------
def discover(port, timeout_ms):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try: s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    except: pass
    try: s.settimeout(max(100, timeout_ms)/1000.0)
    except: pass
    pkt = json_dumps({"op":"ping","seq":99})
    try: s.sendto(pkt, ("255.255.255.255", int(port)))
    except: pass
    found, t0 = [], time.time()
    while (time.time() - t0) < (timeout_ms/1000.0):
        try:
            data, addr = s.recvfrom(4096)
            obj = json_loads(data)
            if isinstance(obj, dict) and obj.get("ok"):
                name = obj.get("name","OXFP")
                ip = obj.get("ip", addr[0] if addr else "")
                if ip: found.append({"name": name, "ip": ip})
        except socket.timeout:
            break
        except:
            break
    try: s.close()
    except: pass
    uniq = {}
    for d in found: uniq[d["ip"]] = d
    out = uniq.values()
    try: out.sort(key=lambda x: x["ip"])
    except: pass
    return out

def autodetect(cfg):
    items = discover(cfg["port"], cfg["timeout_ms"])
    if len(items) == 1:
        cfg["ip"] = items[0]["ip"]; save_cfg(cfg)
        show_notification("Auto-detected %s" % cfg["ip"])
    elif len(items) > 1:
        labels = [u("%s  (%s)" % (d["name"], d["ip"])) for d in items]
        idx = pick_from_list(u("Select OXFP device"), labels)
        if idx >= 0 and idx < len(items):
            cfg["ip"] = items[idx]["ip"]; save_cfg(cfg)
            show_notification("Selected %s" % cfg["ip"])
    else:
        show_notification("No device found (UDP disabled or diff LAN?)", 2500)
    return cfg

# --------------- UI helpers --------------------
def u(s):
    try:
        return s if isinstance(s, unicode) else s.decode("utf-8")
    except:
        try: return unicode(s)
        except: return s

def show_ok(msg, h="OXFP Controller"):
    if xbmcgui:
        try: xbmcgui.Dialog().ok(u(h), u(msg))
        except: pass
    else:
        print("[OK] %s" % msg)

def show_notification(msg, ms=1500):
    if xbmc and hasattr(xbmc, "executebuiltin"):
        try:
            m = msg.replace(",", ";")
            xbmc.executebuiltin('Notification(%s,%s,%d)' % ('OXFP', m, ms))
        except: pass
    else:
        print("[NOTE]", msg)

def ask_text(h, default=""):
    if xbmcgui:
        d = xbmcgui.Dialog()
        try: return d.input(u(h), u(default))
        except TypeError:
            try: return d.input(u(h))
            except: return default
        except: return default
    return default

def ask_ip(default=""):
    if xbmcgui:
        d = xbmcgui.Dialog()
        try: return d.numeric(3, u("Enter Device IP (or cancel)"), u(default))
        except TypeError:
            try: return d.numeric(3, u("Enter Device IP (or cancel)"))
            except: return ask_text("Enter Device IP", default)
    return default

def ask_int(h, default=0, minv=0, maxv=255):
    if xbmcgui:
        d = xbmcgui.Dialog()
        try:
            s = d.numeric(0, u(h), u(str(default)))
            try: v = int(s)
            except: v = default
            if v < minv: v = minv
            if v > maxv: v = maxv
            return v
        except: pass
    return default

def pick_from_list(h, items):
    # Always call select with TWO positional args and unicode items
    if xbmcgui:
        d = xbmcgui.Dialog()
        safe_items = [u(x) for x in items]
        try: return d.select(u(h), safe_items)
        except: return -1
    return -1

# --------------- Simple Color Palette ----------
COLOR_PRESETS = [
    ("Xbox Green",   [0,255,0]),
    ("Red",          [255,0,0]),
    ("Amber/Orange", [255,128,0]),
    ("Yellow",       [255,255,0]),
    ("Blue",         [0,0,255]),
    ("Sky Blue",     [0,128,255]),
    ("Cyan",         [0,255,255]),
    ("Magenta",      [255,0,255]),
    ("Purple",       [128,0,255]),
    ("Pink",         [255,64,128]),
    ("Teal",         [0,255,128]),
    ("White",        [255,255,255]),
    ("Warm White",   [255,200,160]),
    ("Cool White",   [200,230,255]),
    ("Dim White",    [64,64,64]),
    ("Custom (manual RGB)…", None),
]

def pick_color_manual(title, init_rgb):
    r = ask_int("%s - Red (0-255)" % title, init_rgb[0], 0, 255)
    g = ask_int("%s - Green (0-255)" % title, init_rgb[1], 0, 255)
    b = ask_int("%s - Blue (0-255)" % title, init_rgb[2], 0, 255)
    def lim(x): 
        try: x=int(x)
        except: x=0
        if x<0: x=0
        if x>255: x=255
        return x
    return [lim(r), lim(g), lim(b)]

def pick_color_palette(title, init_rgb):
    labels = []
    for name, rgb in COLOR_PRESETS:
        if rgb is None:
            labels.append(u(name))
        else:
            labels.append(u("%s  (%d,%d,%d)") % (name, rgb[0], rgb[1], rgb[2]))
    idx = pick_from_list(title, labels)
    if idx < 0:  # cancel
        return init_rgb
    name, rgb = COLOR_PRESETS[idx]
    if rgb is None:
        return pick_color_manual(title, init_rgb)
    return rgb

def show_config(cfg):
    c = cfg.get("config", cfg)
    lines = []
    lines.append("Mode: %s" % {0:"Stock",1:"Static",2:"Animation"}.get(c.get("mode",0), c.get("mode",0)))
    lines.append("Brightness: %s" % c.get("brightness", 0))
    lines.append("Anim: %s" % c.get("animMode", 0))
    lines.append("Speed: %s" % c.get("animSpeed", 0))
    lines.append("Static Green: %s" % c.get("greenColor"))
    lines.append("Static Red:   %s" % c.get("redColor"))
    lines.append("Static Orange:%s" % c.get("orangeColor"))
    lines.append("Anim A: %s" % c.get("animColorA"))
    lines.append("Anim B: %s" % c.get("animColorB"))
    show_ok("\n".join([str(x) for x in lines]), "Current Config")

# --------------- Helpers: UDP→HTTP fallback ----
def try_set_live(cfg, fields):
    # UDP first
    client = OXFPClient(cfg["ip"], cfg["port"], cfg["timeout_ms"])
    r = client.set_live(fields)
    if r and r.get("ok"): return True
    # HTTP preview as fallback
    resp = http_merge_and_preview(cfg["ip"], cfg["http_port"], cfg["timeout_ms"], fields)
    return bool(resp)

def try_mode(cfg, mode_id):
    client = OXFPClient(cfg["ip"], cfg["port"], cfg["timeout_ms"])
    r = client.mode(mode_id)
    if r and r.get("ok"): return True
    return try_set_live(cfg, {"mode": int(mode_id)})

def try_get_config(cfg):
    client = OXFPClient(cfg["ip"], cfg["port"], cfg["timeout_ms"])
    r = client.get()
    if r and r.get("ok"): return r
    obj = http_get_config(cfg["ip"], cfg["http_port"], cfg["timeout_ms"])
    if isinstance(obj, dict): return {"ok": True, "config": obj}
    return {"ok": False}

def try_preview(cfg, fields, ms):
    client = OXFPClient(cfg["ip"], cfg["port"], cfg["timeout_ms"])
    r = client.preview(fields, previewMs=ms)
    if r and r.get("ok"): return True
    return bool(http_merge_and_preview(cfg["ip"], cfg["http_port"], cfg["timeout_ms"], fields))

def try_save(cfg):
    client = OXFPClient(cfg["ip"], cfg["port"], cfg["timeout_ms"])
    r = client.save()
    if r and r.get("ok"): return True
    return bool(http_merge_and_save(cfg["ip"], cfg["http_port"], cfg["timeout_ms"], None))

def try_reset(cfg):
    client = OXFPClient(cfg["ip"], cfg["port"], cfg["timeout_ms"])
    r = client.reset()
    if r and r.get("ok"): return True
    return bool(http_reset(cfg["ip"], cfg["http_port"], cfg["timeout_ms"]))

# --------------- Main controller ---------------
ANIM_NAMES = [
    "0  ColorBounce","1  Breathing","2  Chase","3  RGBFade","4  Blinking",
    "5  Alternating","6  FireFlicker","7  Plasma","8  Heartbeat","9  OpposedBreath","10 Sparkle",
]
MODE_NAMES = ["0  Stock (OG)", "1  Static", "2  Animation"]

def ensure_device(cfg, interactive=True):
    if cfg["ip"]: return cfg
    cfg = autodetect(cfg)
    if cfg["ip"] or not interactive: return cfg
    ip = ask_ip("")
    if ip:
        cfg["ip"] = ip; save_cfg(cfg)
    return cfg

def do_get(cfg):
    r = try_get_config(cfg)
    if not r or not r.get("ok"):
        show_ok("Failed to get config from %s" % cfg["ip"]); return
    show_config(r)

def do_mode(cfg):
    idx = pick_from_list("Select Mode", MODE_NAMES)
    if idx < 0: return
    ok = try_mode(cfg, idx)
    show_notification("Mode set: %s" % MODE_NAMES[idx] if ok else "Failed")

def do_brightness(cfg):
    v = ask_int("Brightness (1-255)", 180, 1, 255)
    ok = try_set_live(cfg, {"brightness": v})
    show_notification("Brightness set" if ok else "Failed")

def do_static_colors(cfg):
    # Ensure static mode
    try_set_live(cfg, {"mode":1})
    g = pick_color_palette("Static GREEN", [0,255,0])
    r = pick_color_palette("Static RED",   [255,0,0])
    o = pick_color_palette("Static ORANGE",[255,128,0])
    ok = try_set_live(cfg, {"mode":1,"greenColor":g,"redColor":r,"orangeColor":o})
    show_notification("Static colors applied" if ok else "Failed to apply static colors")

def do_anim_settings(cfg):
    try_set_live(cfg, {"mode":2})
    idx = pick_from_list("Animation Mode", ANIM_NAMES)
    if idx < 0: return
    spd = ask_int("Animation Speed (1-10)", 5, 1, 10)
    a = pick_color_palette("Anim Color A", [0,128,255])
    b = pick_color_palette("Anim Color B", [255,0,128])
    ok = try_set_live(cfg, {"mode":2,"animMode":idx,"animSpeed":spd,"animColorA":a,"animColorB":b})
    show_notification("Animation applied" if ok else "Failed to apply animation")

def do_preview(cfg):
    ms = ask_int("Preview milliseconds", 8000, 500, 20000)
    idx = pick_from_list("Preview Which?", ["Current RAM config", "Custom Quick Plasma"])
    if idx == 0:
        ok = try_preview(cfg, {}, ms)
    else:
        pkt = {"mode":2,"animMode":7,"animSpeed":6,"brightness":180,
               "animColorA":[0,128,255],"animColorB":[255,0,128]}
        ok = try_preview(cfg, pkt, ms)
    show_notification("Preview sent" if ok else "Preview failed")

def do_save(cfg):
    ok = try_save(cfg)
    show_notification("Saved" if ok else "Save failed")

def do_reset(cfg):
    ok = try_reset(cfg)
    show_notification("Defaults applied" if ok else "Reset failed")

def do_identify(cfg):
    ms = ask_int("Identify (ms)", 1500, 200, 5000)
    client = OXFPClient(cfg["ip"], cfg["port"], cfg["timeout_ms"])
    r = client.identify(ms=ms)
    show_notification("Identifying…" if (r and r.get("ok")) else "Identify failed (UDP only)")

def main_menu():
    cfg = load_cfg()
    if not cfg["ip"]:
        cfg = autodetect(cfg)

    while True:
        ip_disp = cfg["ip"] or "(auto-detect)"
        menu = [
            "Discover devices (broadcast)",
            "Set device IP (current: %s)" % ip_disp,
            "Get current config",
            "Set Mode",
            "Set Brightness",
            "Set Static Colors",
            "Set Animation (mode/speed/colors)",
            "Preview",
            "Save to NVS",
            "Reset to defaults",
            "Identify (blink)",
            "Quit"
        ]
        sel = pick_from_list("OXFP Controller", menu)
        if sel == -1 or sel == len(menu)-1: break

        if sel == 0:
            cfg = autodetect(cfg); continue
        if sel == 1:
            ip = ask_ip(cfg["ip"])
            if ip:
                cfg["ip"] = ip; save_cfg(cfg); show_notification("IP set to %s" % ip)
            continue

        cfg = ensure_device(cfg)
        if not cfg["ip"]:
            show_ok("No device selected.\nUse Discover or Set IP."); continue

        if sel == 2:   do_get(cfg)
        elif sel == 3: do_mode(cfg)
        elif sel == 4: do_brightness(cfg)
        elif sel == 5: do_static_colors(cfg)
        elif sel == 6: do_anim_settings(cfg)
        elif sel == 7: do_preview(cfg)
        elif sel == 8: do_save(cfg)
        elif sel == 9: do_reset(cfg)
        elif sel == 10: do_identify(cfg)

if __name__ == "__main__":
    try:
        main_menu()
    except Exception as e:
        try: show_ok("Error: %s" % str(e))
        except: print("Error:", e)
