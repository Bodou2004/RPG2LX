#pragma once

namespace rpg2lx {

inline constexpr const char* kMvPreCoreBootstrapJs = R"JS(
(function(global) {
    global.window = global;
    global.self = global;
    global.top = global;

    var __timers = [];
    var __nextTimerId = 1;
    var __rafCallbacks = [];
    var __nextRafId = 1;
    var __asyncQueue = [];

    global.__queueAsync = function(fn) {
        __asyncQueue.push(fn);
    };

    global.performance = {
        now: function() { return __rpg2lx.nowMs(); }
    };

    global.setTimeout = function(fn, delay) {
        var id = __nextTimerId++;
        var target = __rpg2lx.nowMs() + Math.max(0, Number(delay) || 0);
        __timers.push({ id: id, fn: fn, target: target, interval: 0, active: true });
        return id;
    };
    global.clearTimeout = function(id) {
        for (var i = 0; i < __timers.length; i++) {
            if (__timers[i].id === id) { __timers[i].active = false; break; }
        }
    };
    global.setInterval = function(fn, delay) {
        var id = __nextTimerId++;
        var step = Math.max(1, Number(delay) || 16);
        __timers.push({ id: id, fn: fn, target: __rpg2lx.nowMs() + step, interval: step, active: true });
        return id;
    };
    global.clearInterval = global.clearTimeout;

    global.requestAnimationFrame = function(fn) {
        var id = __nextRafId++;
        __rafCallbacks.push({ id: id, fn: fn, active: true });
        return id;
    };
    global.cancelAnimationFrame = function(id) {
        for (var i = 0; i < __rafCallbacks.length; i++) {
            if (__rafCallbacks[i].id === id) { __rafCallbacks[i].active = false; break; }
        }
    };

    global.__drainFrameCallbacks = function() {
        // Drain queued async I/O & image/XHR/audio callbacks (including chained ones)
        var guard = 0;
        while (__asyncQueue.length > 0 && guard++ < 512) {
            var batch = __asyncQueue.splice(0, __asyncQueue.length);
            for (var i = 0; i < batch.length; i++) {
                try { batch[i](); } catch (e) {
                    __rpg2lx.reportError(e && e.stack ? String(e.stack) : String(e));
                }
            }
        }
        // Drain due timers
        var now = __rpg2lx.nowMs();
        for (var t = 0; t < __timers.length; t++) {
            var tm = __timers[t];
            if (!tm.active) continue;
            if (now >= tm.target) {
                if (tm.interval > 0) {
                    tm.target = now + tm.interval;
                } else {
                    tm.active = false;
                }
                try { tm.fn(); } catch (e) {
                    __rpg2lx.reportError(e && e.stack ? String(e.stack) : String(e));
                }
            }
        }
        if (__timers.length > 64) {
            __timers = __timers.filter(function(x) { return x.active; });
        }
    };

    global.console = {
        log: function() { __rpg2lx.log(0, Array.prototype.join.call(arguments, ' ')); },
        info: function() { __rpg2lx.log(0, Array.prototype.join.call(arguments, ' ')); },
        warn: function() { __rpg2lx.log(1, Array.prototype.join.call(arguments, ' ')); },
        error: function() { __rpg2lx.log(2, Array.prototype.join.call(arguments, ' ')); }
    };

    global.alert = function(msg) { __rpg2lx.log(1, 'ALERT: ' + msg); };
    global.close = function() { __rpg2lx.requestExit(); };
    global.moveBy = function() {};
    global.resizeBy = function() {};
    global.innerWidth = 816;
    global.innerHeight = 624;
    global.pageXOffset = 0;
    global.pageYOffset = 0;
    global.addEventListener = function() {};
    global.removeEventListener = function() {};

    global.location = {
        href: 'file:///game/index.html',
        search: '',
        hash: '',
        reload: function() {}
    };

    global.navigator = {
        userAgent: 'Mozilla/5.0 (Linux; aarch64) NWJS/0.44 RPG2LX/0.5',
        language: 'es-ES',
        onLine: true,
        getGamepads: function() { return []; }
    };

    global.URL = {
        createObjectURL: function(blob) { return blob && blob.__url ? blob.__url : 'blob:rpg2lx'; },
        revokeObjectURL: function() {}
    };
    global.Blob = function(parts) {
        this.parts = parts || [];
    };

    // Persistent localStorage fallback
    var __storageMap = null;
    function ensureStorage() {
        if (__storageMap !== null) return;
        __storageMap = {};
        try {
            var raw = __rpg2lx.saveRead('__webstorage.json');
            if (raw) __storageMap = JSON.parse(raw) || {};
        } catch (e) { __storageMap = {}; }
    }
    function flushStorage() {
        try { __rpg2lx.saveWrite('__webstorage.json', JSON.stringify(__storageMap)); } catch (e) {}
    }
    global.localStorage = {
        getItem: function(k) { ensureStorage(); return Object.prototype.hasOwnProperty.call(__storageMap, k) ? __storageMap[k] : null; },
        setItem: function(k, v) { ensureStorage(); __storageMap[String(k)] = String(v); flushStorage(); },
        removeItem: function(k) { ensureStorage(); delete __storageMap[String(k)]; flushStorage(); },
        clear: function() { __storageMap = {}; flushStorage(); }
    };

    // Node / NW.js compatibility (sandboxed saves)
    global.process = {
        platform: 'linux',
        arch: 'arm64',
        versions: { node: '12.13.0', nw: '0.44.5' },
        mainModule: { filename: '/game/index.html' },
        on: function() {}
    };
    var __nwWin = {
        menu: true,
        x: 0, y: 0, width: 816, height: 624,
        focus: function() {},
        showDevTools: function() {},
        on: function() {},
        moveBy: function() {},
        resizeBy: function() {},
        close: function() { __rpg2lx.requestExit(); }
    };
    global.nw = {
        App: { argv: [], quit: function() { __rpg2lx.requestExit(); } },
        Window: { get: function() { return __nwWin; } },
        Menu: function() { this.createMacBuiltin = function() {}; }
    };

    var __fsModule = {
        existsSync: function(p) { return __rpg2lx.fsExists(String(p)); },
        mkdirSync: function(p) { return __rpg2lx.fsMkdir(String(p)); },
        readFileSync: function(p, opts) {
            var isUtf8 = (typeof opts === 'string') || (opts && opts.encoding);
            if (isUtf8) return __rpg2lx.fsReadText(String(p));
            var ab = __rpg2lx.fsReadBytes(String(p));
            return ab ? new Uint8Array(ab) : null;
        },
        writeFileSync: function(p, data) {
            __rpg2lx.fsWriteText(String(p), typeof data === 'string' ? data : String(data));
        },
        unlinkSync: function(p) { __rpg2lx.fsRemove(String(p)); },
        readdirSync: function(p) { return __rpg2lx.fsReaddir(String(p)); },
        statSync: function(p) {
            var isDir = __rpg2lx.fsIsDir(String(p));
            return { isDirectory: function() { return isDir; }, isFile: function() { return !isDir; } };
        }
    };
    var __pathModule = {
        dirname: function(p) {
            p = String(p).replace(/\\/g, '/');
            var idx = p.lastIndexOf('/');
            return idx < 0 ? '.' : (idx === 0 ? '/' : p.slice(0, idx));
        },
        basename: function(p) {
            p = String(p).replace(/\\/g, '/');
            var idx = p.lastIndexOf('/');
            return idx < 0 ? p : p.slice(idx + 1);
        },
        extname: function(p) {
            var b = __pathModule.basename(p);
            var idx = b.lastIndexOf('.');
            return idx <= 0 ? '' : b.slice(idx);
        },
        join: function() {
            var parts = [];
            for (var i = 0; i < arguments.length; i++) {
                if (arguments[i]) parts.push(String(arguments[i]).replace(/\\/g, '/'));
            }
            return parts.join('/').replace(/\/+/g, '/');
        },
        resolve: function() { return __pathModule.join.apply(null, arguments); }
    };

    global.require = function(name) {
        if (name === 'fs') return __fsModule;
        if (name === 'path') return __pathModule;
        if (name === 'nw.gui') return global.nw;
        return {};
    };

    // Minimal DOM element & document shim
    function makeElement(tag) {
        var el = {
            tagName: String(tag || 'div').toUpperCase(),
            style: {},
            children: [],
            width: 0,
            height: 0,
            src: '',
            type: '',
            innerHTML: '',
            textContent: '',
            appendChild: function(child) {
                this.children.push(child);
                if (child && child.tagName === 'SCRIPT' && child.src) {
                    var prevScript = global.document.currentScript;
                    global.document.currentScript = child;
                    var ok = __rpg2lx.loadScriptSync(child.src);
                    global.document.currentScript = prevScript;
                    if (!ok && typeof child.onerror === 'function') {
                        child.onerror({ target: child });
                    } else if (ok && typeof child.onload === 'function') {
                        child.onload({ target: child });
                    }
                }
                return child;
            },
            removeChild: function(child) {
                var i = this.children.indexOf(child);
                if (i >= 0) this.children.splice(i, 1);
                return child;
            },
            addEventListener: function() {},
            removeEventListener: function() {},
            getContext: function() { return global.__createDummy2DContext(this); },
            canPlayType: function(mime) {
                if (String(mime).indexOf('ogg') >= 0) return 'probably';
                return '';
            },
            requestFullscreen: function() {}
        };
        return el;
    }

    global.__createDummy2DContext = function(canvas) {
        return {
            canvas: canvas,
            font: '28px GameFont',
            textAlign: 'left',
            textBaseline: 'alphabetic',
            fillStyle: '#ffffff',
            strokeStyle: '#000000',
            lineWidth: 4,
            lineJoin: 'round',
            globalAlpha: 1,
            globalCompositeOperation: 'source-over',
            save: function() {},
            restore: function() {},
            scale: function() {},
            translate: function() {},
            rotate: function() {},
            setTransform: function() {},
            fillRect: function() {},
            clearRect: function() {},
            strokeRect: function() {},
            beginPath: function() {},
            arc: function() {},
            fill: function() {},
            stroke: function() {},
            clip: function() {},
            drawImage: function() {},
            fillText: function() {},
            strokeText: function() {},
            measureText: function(t) { return { width: __rpg2lx.measureText(String(t || ''), 28, false) }; },
            createLinearGradient: function() { return { addColorStop: function() {} }; },
            getImageData: function(x, y, w, h) {
                return { width: Math.max(1, w|0), height: Math.max(1, h|0), data: new Uint8ClampedArray(Math.max(1, w|0) * Math.max(1, h|0) * 4) };
            },
            putImageData: function() {}
        };
    };

    var __head = makeElement('head');
    var __body = makeElement('body');
    global.document = {
        title: 'RPG2LX',
        head: __head,
        body: __body,
        documentElement: makeElement('html'),
        visibilityState: 'visible',
        currentScript: null,
        fonts: {
            ready: Promise.resolve([])
        },
        createElement: function(tag) {
            if (String(tag).toLowerCase() === 'canvas') {
                return global.__createNativeCanvas(1, 1);
            }
            return makeElement(tag);
        },
        getElementById: function() { return null; },
        getElementsByTagName: function(tag) {
            var t = String(tag).toLowerCase();
            if (t === 'head') return [__head];
            if (t === 'body') return [__body];
            if (t === 'script') return [{ src: 'js/rpg_core.js' }];
            return [];
        },
        addEventListener: function() {},
        removeEventListener: function() {}
    };

    // XMLHttpRequest backed by C++ project file reader
    function XMLHttpRequest() {
        this.status = 0;
        this.readyState = 0;
        this.responseType = '';
        this.response = null;
        this.responseText = '';
        this.onload = null;
        this.onerror = null;
        this.onreadystatechange = null;
        this._url = '';
    }
    XMLHttpRequest.prototype.open = function(method, url) {
        this._url = String(url || '');
        this.readyState = 1;
    };
    XMLHttpRequest.prototype.overrideMimeType = function() {};
    XMLHttpRequest.prototype.setRequestHeader = function() {};
    XMLHttpRequest.prototype.send = function() {
        var self = this;
        var url = self._url;
        var wantBytes = (self.responseType === 'arraybuffer');
        __queueAsync(function() {
            if (wantBytes) {
                var ab = __rpg2lx.readProjectBytes(url);
                if (ab !== null && ab !== undefined) {
                    self.status = 200;
                    self.readyState = 4;
                    self.response = ab;
                    if (typeof self.onreadystatechange === 'function') self.onreadystatechange();
                    if (typeof self.onload === 'function') self.onload();
                } else {
                    self.status = 404;
                    self.readyState = 4;
                    if (typeof self.onerror === 'function') self.onerror();
                }
            } else {
                var txt = __rpg2lx.readProjectText(url);
                if (txt !== null && txt !== undefined) {
                    self.status = 200;
                    self.readyState = 4;
                    self.responseText = txt;
                    self.response = txt;
                    if (typeof self.onreadystatechange === 'function') self.onreadystatechange();
                    if (typeof self.onload === 'function') self.onload();
                } else {
                    self.status = 404;
                    self.readyState = 4;
                    if (typeof self.onerror === 'function') self.onerror();
                }
            }
        });
    };
    global.XMLHttpRequest = XMLHttpRequest;

    // HTMLImageElement shim
    function Image() {
        this._src = '';
        this.width = 0;
        this.height = 0;
        this.complete = false;
        this.onload = null;
        this.onerror = null;
        this._listeners = { load: [], error: [] };
        this._nbmpRef = null;
    }
    Image.prototype.addEventListener = function(type, fn) {
        if (this._listeners[type] && typeof fn === 'function') {
            this._listeners[type].push(fn);
        }
    };
    Image.prototype.removeEventListener = function(type, fn) {
        var arr = this._listeners[type];
        if (!arr) return;
        var idx = arr.indexOf(fn);
        if (idx >= 0) arr.splice(idx, 1);
    };
    Object.defineProperty(Image.prototype, 'src', {
        get: function() { return this._src; },
        set: function(val) {
            this._src = String(val || '');
            if (!this._src) {
                this._nbmpRef = null;
                this.complete = false;
                return;
            }
            var self = this;
            var targetUrl = this._src;
            __queueAsync(function() {
                if (self._src !== targetUrl) return;
                var ref = __rpg2lx.bmpLoadFile(targetUrl);
                if (ref) {
                    self._nbmpRef = ref;
                    self.width = __rpg2lx.bmpWidth(ref);
                    self.height = __rpg2lx.bmpHeight(ref);
                    self.complete = true;
                    if (typeof self.onload === 'function') self.onload();
                    var ls = self._listeners.load.slice();
                    for (var i = 0; i < ls.length; i++) ls[i].call(self);
                } else {
                    self.complete = false;
                    if (typeof self.onerror === 'function') self.onerror();
                    var es = self._listeners.error.slice();
                    for (var j = 0; j < es.length; j++) es[j].call(self);
                }
            });
        }
    });
    global.Image = Image;

    // Native-backed HTMLCanvasElement + CanvasRenderingContext2D
    function parseFontSpec(fontStr) {
        var s = String(fontStr || '28px GameFont');
        var italic = /italic/i.test(s);
        var m = s.match(/([0-9]+(?:\.[0-9]+)?)px/i);
        var size = m ? Math.round( parseFloat(m[1]) ) : 28;
        return { size: size, italic: italic };
    }

    global.__createNativeCanvas = function(w, h, existingRef) {
        var cw = Math.max(1, w | 0);
        var ch = Math.max(1, h | 0);
        var ref = existingRef || __rpg2lx.bmpCreate(cw, ch);
        var ctx = null;
        var canvas = {
            tagName: 'CANVAS',
            style: {},
            _nbmpRef: ref,
            getContext: function(type) {
                if (type !== '2d') return null;
                if (!ctx) ctx = make2DContext(canvas);
                return ctx;
            }
        };
        Object.defineProperty(canvas, 'width', {
            get: function() { return __rpg2lx.bmpWidth(this._nbmpRef); },
            set: function(nw) { __rpg2lx.bmpResize(this._nbmpRef, Math.max(1, nw | 0), __rpg2lx.bmpHeight(this._nbmpRef)); }
        });
        Object.defineProperty(canvas, 'height', {
            get: function() { return __rpg2lx.bmpHeight(this._nbmpRef); },
            set: function(nh) { __rpg2lx.bmpResize(this._nbmpRef, __rpg2lx.bmpWidth(this._nbmpRef), Math.max(1, nh | 0)); }
        });
        return canvas;
    };

    function make2DContext(canvas) {
        var stack = [];
        var ctx = {
            canvas: canvas,
            font: '28px GameFont',
            textAlign: 'left',
            textBaseline: 'alphabetic',
            fillStyle: '#ffffff',
            strokeStyle: 'rgba(0,0,0,0.5)',
            lineWidth: 4,
            lineJoin: 'round',
            globalAlpha: 1.0,
            globalCompositeOperation: 'source-over',
            _arc: null,
            save: function() {
                stack.push({
                    font: this.font, textAlign: this.textAlign, textBaseline: this.textBaseline,
                    fillStyle: this.fillStyle, strokeStyle: this.strokeStyle, lineWidth: this.lineWidth,
                    globalAlpha: this.globalAlpha, globalCompositeOperation: this.globalCompositeOperation
                });
            },
            restore: function() {
                var st = stack.pop();
                if (st) {
                    this.font = st.font; this.textAlign = st.textAlign; this.textBaseline = st.textBaseline;
                    this.fillStyle = st.fillStyle; this.strokeStyle = st.strokeStyle; this.lineWidth = st.lineWidth;
                    this.globalAlpha = st.globalAlpha; this.globalCompositeOperation = st.globalCompositeOperation;
                }
            },
            scale: function() {},
            translate: function() {},
            rotate: function() {},
            setTransform: function() {},
            clearRect: function(x, y, w, h) {
                __rpg2lx.bmpClearRect(canvas._nbmpRef, x, y, w, h);
            },
            fillRect: function(x, y, w, h) {
                var fs = this.fillStyle;
                var op = Math.round(Math.max(0, Math.min(1, this.globalAlpha)) * 255);
                if (fs && fs.__isGrad) {
                    __rpg2lx.bmpGradientFillRect(canvas._nbmpRef, x, y, w, h, fs.c0, fs.c1, fs.vertical, op);
                } else {
                    __rpg2lx.bmpFillRect(canvas._nbmpRef, x, y, w, h, String(fs || '#ffffff'), op, String(this.globalCompositeOperation || 'source-over'));
                }
            },
            strokeRect: function() {},
            createLinearGradient: function(x0, y0, x1, y1) {
                return {
                    __isGrad: true,
                    vertical: Math.abs(y1 - y0) >= Math.abs(x1 - x0),
                    c0: '#000000',
                    c1: '#ffffff',
                    addColorStop: function(stop, col) {
                        if (stop <= 0.5) this.c0 = String(col);
                        else this.c1 = String(col);
                    }
                };
            },
            beginPath: function() { this._arc = null; },
            arc: function(x, y, r) { this._arc = { x: x, y: y, r: r }; },
            fill: function() {
                if (this._arc) {
                    var op = Math.round(Math.max(0, Math.min(1, this.globalAlpha)) * 255);
                    __rpg2lx.bmpDrawCircle(canvas._nbmpRef, this._arc.x, this._arc.y, this._arc.r, String(this.fillStyle || '#ffffff'), op);
                }
            },
            stroke: function() {},
            clip: function() {},
            drawImage: function(src, a, b, c, d, e, f, g, h) {
                var srcRef = src && src._nbmpRef;
                if (!srcRef) return;
                var op = Math.round(Math.max(0, Math.min(1, this.globalAlpha)) * 255);
                var sw = __rpg2lx.bmpWidth(srcRef), sh = __rpg2lx.bmpHeight(srcRef);
                if (arguments.length === 3) {
                    __rpg2lx.bmpBlt(canvas._nbmpRef, srcRef, 0, 0, sw, sh, a, b, sw, sh, op, this.globalCompositeOperation);
                } else if (arguments.length === 5) {
                    __rpg2lx.bmpBlt(canvas._nbmpRef, srcRef, 0, 0, sw, sh, a, b, c, d, op, this.globalCompositeOperation);
                } else if (arguments.length >= 9) {
                    __rpg2lx.bmpBlt(canvas._nbmpRef, srcRef, a, b, c, d, e, f, g, h, op, this.globalCompositeOperation);
                }
            },
            measureText: function(text) {
                var fp = parseFontSpec(this.font);
                return { width: __rpg2lx.measureText(String(text === undefined ? '' : text), fp.size, fp.italic) };
            },
            strokeText: function(text, tx, ty, maxW) {
                var fp = parseFontSpec(this.font);
                var op = Math.round(Math.max(0, Math.min(1, this.globalAlpha)) * 255);
                __rpg2lx.bmpCanvasText(canvas._nbmpRef, String(text === undefined ? '' : text), tx, ty,
                    maxW || 0, String(this.textAlign || 'left'), fp.size, fp.italic,
                    'transparent', String(this.strokeStyle || 'rgba(0,0,0,0.5)'), Math.max(1, Math.round(this.lineWidth || 4)), op);
            },
            fillText: function(text, tx, ty, maxW) {
                var fp = parseFontSpec(this.font);
                var op = Math.round(Math.max(0, Math.min(1, this.globalAlpha)) * 255);
                __rpg2lx.bmpCanvasText(canvas._nbmpRef, String(text === undefined ? '' : text), tx, ty,
                    maxW || 0, String(this.textAlign || 'left'), fp.size, fp.italic,
                    String(this.fillStyle || '#ffffff'), 'transparent', 0, op);
            },
            getImageData: function(x, y, w, h) {
                var iw = Math.max(1, w | 0), ih = Math.max(1, h | 0);
                var ab = __rpg2lx.bmpGetPixels(canvas._nbmpRef, x | 0, y | 0, iw, ih);
                return { width: iw, height: ih, data: new Uint8ClampedArray(ab) };
            },
            putImageData: function(imgData, dx, dy) {
                if (!imgData || !imgData.data) return;
                __rpg2lx.bmpPutPixels(canvas._nbmpRef, dx | 0, dy | 0, imgData.width | 0, imgData.height | 0, imgData.data.buffer);
            }
        };
        return ctx;
    }

    // Minimal PIXI v4 scene-graph API required by rpg_core.js & plugins
    function Point(x, y) { this.x = x || 0; this.y = y || 0; }
    Point.prototype.set = function(x, y) { this.x = x || 0; this.y = (y === undefined ? this.x : y); };
    Point.prototype.copy = function(p) { this.x = p.x; this.y = p.y; };
    Point.prototype.clone = function() { return new Point(this.x, this.y); };

    function Rectangle(x, y, w, h) {
        this.x = x || 0; this.y = y || 0; this.width = w || 0; this.height = h || 0;
    }
    Rectangle.prototype.clone = function() { return new Rectangle(this.x, this.y, this.width, this.height); };
    Rectangle.prototype.contains = function(x, y) {
        return this.width > 0 && this.height > 0 && x >= this.x && x < this.x + this.width && y >= this.y && y < this.y + this.height;
    };
    Rectangle.prototype.pad = function(px, py) {
        px = px || 0; py = (py === undefined ? px : py);
        this.x -= px; this.width += px * 2;
        this.y -= py; this.height += py * 2;
    };
    Rectangle.EMPTY = new Rectangle(0, 0, 0, 0);
    Rectangle.emptyRectangle = Rectangle.EMPTY;

    function Matrix() { this.a = 1; this.b = 0; this.c = 0; this.d = 1; this.tx = 0; this.ty = 0; }
    Matrix.prototype.identity = function() { this.a = 1; this.b = 0; this.c = 0; this.d = 1; this.tx = 0; this.ty = 0; return this; };

    function BaseTexture(source) {
        this.source = source || null;
        this.width = source ? (source.width || 0) : 0;
        this.height = source ? (source.height || 0) : 0;
        this.mipmap = false;
        this.scaleMode = 0;
    }
    BaseTexture.prototype.update = function() {
        if (this.source) {
            this.width = this.source.width || 0;
            this.height = this.source.height || 0;
        }
    };
    BaseTexture.prototype.destroy = function() { this.source = null; };

    function Texture(baseTexture, frame) {
        this.baseTexture = baseTexture || new BaseTexture();
        this.frame = frame ? frame.clone() : new Rectangle(0, 0, this.baseTexture.width, this.baseTexture.height);
        this._updateID = 0;
    }
    Texture.prototype.destroy = function() {};

    function Container() {
        this.children = [];
        this.parent = null;
        this.x = 0;
        this.y = 0;
        this.scale = new Point(1, 1);
        this.pivot = new Point(0, 0);
        this.skew = new Point(0, 0);
        this.rotation = 0;
        this.alpha = 1;
        this.visible = true;
        this.renderable = true;
        this.interactive = false;
        this.worldAlpha = 1;
        this.worldVisible = true;
        this.worldTransform = new Matrix();
        this.filters = null;
        this.filterArea = null;
        this._events = null;
    }
    Container.prototype.on = function(ev, fn) {
        if (typeof fn === 'function') {
            if (!this._events) this._events = {};
            if (!this._events[ev]) this._events[ev] = [];
            this._events[ev].push(fn);
        }
        return this;
    };
    Container.prototype.once = Container.prototype.on;
    Container.prototype.off = function(ev, fn) {
        if (this._events && this._events[ev]) {
            var idx = this._events[ev].indexOf(fn);
            if (idx >= 0) this._events[ev].splice(idx, 1);
        }
        return this;
    };
    Container.prototype.emit = function(ev) {
        if (this._events && this._events[ev]) {
            var ls = this._events[ev].slice();
            for (var i = 0; i < ls.length; i++) ls[i].call(this);
            return true;
        }
        return false;
    };
    Object.defineProperty(Container.prototype, 'position', {
        get: function() { return this; },
        configurable: true
    });
    Container.prototype.addChild = function(child) {
        if (!child) return child;
        if (child.parent) child.parent.removeChild(child);
        child.parent = this;
        this.children.push(child);
        return child;
    };
    Container.prototype.addChildAt = function(child, index) {
        if (!child) return child;
        if (child.parent) child.parent.removeChild(child);
        child.parent = this;
        var idx = Math.max(0, Math.min(index | 0, this.children.length));
        this.children.splice(idx, 0, child);
        return child;
    };
    Container.prototype.removeChild = function(child) {
        var idx = this.children.indexOf(child);
        if (idx >= 0) {
            this.children.splice(idx, 1);
            child.parent = null;
            if (child.emit) child.emit('removed');
        }
        return child;
    };
    Container.prototype.removeChildAt = function(index) {
        if (index < 0 || index >= this.children.length) return null;
        var child = this.children.splice(index, 1)[0];
        if (child) {
            child.parent = null;
            if (child.emit) child.emit('removed');
        }
        return child;
    };
    Container.prototype.removeChildren = function() {
        var old = this.children;
        this.children = [];
        for (var i = 0; i < old.length; i++) {
            if (old[i]) {
                old[i].parent = null;
                if (old[i].emit) old[i].emit('removed');
            }
        }
        return old;
    };
    Container.prototype.getChildIndex = function(child) { return this.children.indexOf(child); };
    Container.prototype.setChildIndex = function(child, index) {
        var cur = this.children.indexOf(child);
        if (cur >= 0) {
            this.children.splice(cur, 1);
            this.children.splice(Math.max(0, Math.min(index | 0, this.children.length)), 0, child);
        }
    };
    Container.prototype.updateTransform = function() {
        var p = this.parent;
        this.worldAlpha = p ? (p.worldAlpha * this.alpha) : this.alpha;
        this.worldVisible = p ? (p.worldVisible && this.visible) : this.visible;
        for (var i = 0, n = this.children.length; i < n; i++) {
            var ch = this.children[i];
            if (ch && ch.visible && ch.updateTransform) ch.updateTransform();
        }
    };
    Container.prototype.destroy = function() { this.removeChildren(); };
    Container.prototype.renderCanvas = function() {};
    Container.prototype.renderWebGL = function() {};
    Container.prototype._renderCanvas = function() {};
    Container.prototype._renderWebGL = function() {};

    function PixiSprite(texture) {
        Container.call(this);
        this.anchor = new Point(0, 0);
        this.texture = texture || new Texture();
        this.blendMode = 0;
        this.tint = 0xFFFFFF;
        this.pluginName = 'sprite';
    }
    PixiSprite.prototype = Object.create(Container.prototype);
    PixiSprite.prototype.constructor = PixiSprite;
    PixiSprite.prototype.calculateVertices = function() {};

    function PixiTilingSprite(texture, width, height) {
        PixiSprite.call(this, texture);
        this._width = width || 0;
        this._height = height || 0;
        this.tilePosition = new Point(0, 0);
        this.tileScale = new Point(1, 1);
        this._isTilingSprite = true;
    }
    PixiTilingSprite.prototype = Object.create(PixiSprite.prototype);
    PixiTilingSprite.prototype.constructor = PixiTilingSprite;
    Object.defineProperty(PixiTilingSprite.prototype, 'width', {
        get: function() { return this._width; },
        set: function(v) { this._width = v; },
        configurable: true
    });
    Object.defineProperty(PixiTilingSprite.prototype, 'height', {
        get: function() { return this._height; },
        set: function(v) { this._height = v; },
        configurable: true
    });

    function PixiGraphics() {
        Container.call(this);
        this._isPixiGraphics = true;
        this.blendMode = 0;
        this._fillColor = 0;
        this._fillAlpha = 1;
        this.graphicsData = [{ shape: new Rectangle(0, 0, 0, 0) }];
    }
    PixiGraphics.prototype = Object.create(Container.prototype);
    PixiGraphics.prototype.constructor = PixiGraphics;
    PixiGraphics.prototype.clear = function() { this.graphicsData = [{ shape: new Rectangle(0, 0, 0, 0) }]; return this; };
    PixiGraphics.prototype.beginFill = function(c, a) { this._fillColor = c || 0; this._fillAlpha = (a === undefined ? 1 : a); return this; };
    PixiGraphics.prototype.drawRect = function(x, y, w, h) {
        if (!this.graphicsData || !this.graphicsData[0]) this.graphicsData = [{ shape: new Rectangle(x, y, w, h) }];
        else {
            var s = this.graphicsData[0].shape;
            s.x = x || 0; s.y = y || 0; s.width = w || 0; s.height = h || 0;
        }
        return this;
    };
    PixiGraphics.prototype.endFill = function() { return this; };

    function ColorMatrixFilter() { this.matrix = [1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,1,0]; }
    ColorMatrixFilter.prototype.reset = function() { this.matrix = [1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,1,0]; };
    ColorMatrixFilter.prototype._loadMatrix = function(m) { this.matrix = m; };
    ColorMatrixFilter.prototype.hue = function() {};
    ColorMatrixFilter.prototype.saturate = function() {};

    function VoidFilter() {}

    function NativeCanvasRenderer(w, h) {
        this.type = 2; // CANVAS
        this.width = w || 816;
        this.height = h || 624;
        this.resolution = 1;
        this.view = global.__createNativeCanvas(this.width, this.height);
        this.context = this.view.getContext('2d');
        this.plugins = { sprite: { render: function(){} }, picture: { render: function(){}, drawModes: {} }, tilemap: {} };
    }
    NativeCanvasRenderer.prototype.resize = function(w, h) {
        this.width = w; this.height = h;
    };
    NativeCanvasRenderer.prototype.render = function(stage, renderTexture) {
        if (!stage) return;
        if (stage.updateTransform) stage.updateTransform();
        if (renderTexture && renderTexture._nbmpRef) {
            __rpg2lx.renderStageToBitmap(stage, renderTexture._nbmpRef, this.width, this.height);
        } else {
            __rpg2lx.renderStage(stage, this.width, this.height);
        }
    };

    global.PIXI = {
        VERSION: '4.5.4',
        BLEND_MODES: { NORMAL: 0, ADD: 1, MULTIPLY: 2, SCREEN: 3 },
        blendModes: { NORMAL: 0, ADD: 1, MULTIPLY: 2, SCREEN: 3 },
        SCALE_MODES: { DEFAULT: 0, LINEAR: 0, NEAREST: 1 },
        GC_MODES: { AUTO: 0, MANUAL: 1 },
        settings: { GC_MODE: 0 },
        RENDERER_TYPE: { UNKNOWN: 0, WEBGL: 1, CANVAS: 2 },
        Point: Point,
        ObservablePoint: Point,
        Rectangle: Rectangle,
        Matrix: Matrix,
        BaseTexture: BaseTexture,
        Texture: Texture,
        RenderTexture: {
            create: function(w, h) {
                var c = global.__createNativeCanvas(w || 816, h || 624);
                var bt = new BaseTexture(c);
                bt._canvasRenderTarget = { canvas: c };
                return {
                    width: w || 816, height: h || 624,
                    baseTexture: bt,
                    _nbmpRef: c._nbmpRef,
                    destroy: function() {}
                };
            }
        },
        DisplayObject: Container,
        Container: Container,
        Sprite: PixiSprite,
        Graphics: PixiGraphics,
        CanvasRenderer: NativeCanvasRenderer,
        WebGLRenderer: NativeCanvasRenderer,
        autoDetectRenderer: function(w, h) { return new NativeCanvasRenderer(w, h); },
        extras: {
            TilingSprite: PixiTilingSprite,
            PictureTilingSprite: PixiTilingSprite
        },
        filters: {
            Filter: VoidFilter,
            VoidFilter: VoidFilter,
            ColorMatrixFilter: ColorMatrixFilter
        },
        tilemap: {
            ZLayer: Container,
            CompositeRectTileLayer: Container,
            RectTileLayer: Container,
            TileRenderer: { SCALE_MODE: 1, DO_CLEAR: true }
        },
        glCore: {
            GLTexture: { upload: function() {} },
            VertexArrayObject: { FORCE_NATIVE: false }
        },
        utils: { skipHello: function() {} }
    };
})(globalThis);
)JS";

inline constexpr const char* kMvPostCorePatchJs = R"JS(
(function(global) {
    // 1. Accelerate Bitmap drawing directly in C++ (zero JS per-pixel loops!)
    Bitmap.prototype.drawText = function(text, x, y, maxWidth, lineHeight, align) {
        if (text !== undefined) {
            var ref = this.__canvas ? this.__canvas._nbmpRef : (this._image ? this._image._nbmpRef : null);
            if (!ref) {
                ref = this._canvas._nbmpRef;
            } else if (!this.__canvas && this._image) {
                this._createCanvas(this.width, this.height);
                ref = this.__canvas._nbmpRef;
            }
            var op = Math.round(Math.max(0, Math.min(255, this._paintOpacity)));
            __rpg2lx.bmpDrawText(ref, String(text), x || 0, y || 0,
                maxWidth || 0, lineHeight || 0, String(align || 'left'),
                this.fontSize || 28, !!this.fontItalic,
                String(this.textColor || '#ffffff'),
                String(this.outlineColor || 'rgba(0,0,0,0.5)'),
                (this.outlineWidth === undefined ? 4 : this.outlineWidth),
                op);
            this._setDirty();
        }
    };

    Bitmap.prototype.measureTextWidth = function(text) {
        return __rpg2lx.measureText(String(text === undefined ? '' : text), this.fontSize || 28, !!this.fontItalic);
    };

    Bitmap.prototype.blt = function(source, sx, sy, sw, sh, dx, dy, dw, dh) {
        if (!source || !source.isReady()) return;
        dw = (dw === undefined ? sw : dw);
        dh = (dh === undefined ? sh : dh);
        if (sx >= 0 && sy >= 0 && sw > 0 && sh > 0 && dw > 0 && dh > 0 &&
            sx + sw <= source.width && sy + sh <= source.height) {
            var srcRef = source.__canvas ? source.__canvas._nbmpRef : (source._image ? source._image._nbmpRef : null);
            if (!srcRef) return;
            var dstRef = this._canvas._nbmpRef;
            __rpg2lx.bmpBlt(dstRef, srcRef, sx, sy, sw, sh, dx, dy, dw, dh, this._paintOpacity, 'source-over');
            this._setDirty();
        }
    };

    Bitmap.prototype.bltImage = Bitmap.prototype.blt;

    Bitmap.prototype.clearRect = function(x, y, width, height) {
        __rpg2lx.bmpClearRect(this._canvas._nbmpRef, x, y, width, height);
        this._setDirty();
    };

    Bitmap.prototype.fillRect = function(x, y, width, height, color) {
        __rpg2lx.bmpFillRect(this._canvas._nbmpRef, x, y, width, height, String(color || '#ffffff'), this._paintOpacity, 'source-over');
        this._setDirty();
    };

    Bitmap.prototype.gradientFillRect = function(x, y, width, height, color1, color2, vertical) {
        __rpg2lx.bmpGradientFillRect(this._canvas._nbmpRef, x, y, width, height, String(color1 || '#000000'), String(color2 || '#ffffff'), !!vertical, this._paintOpacity);
        this._setDirty();
    };

    Bitmap.prototype.drawCircle = function(x, y, radius, color) {
        __rpg2lx.bmpDrawCircle(this._canvas._nbmpRef, x, y, radius, String(color || '#ffffff'), this._paintOpacity);
        this._setDirty();
    };

    Bitmap.prototype.getPixel = function(x, y) {
        var srcRef = this.__canvas ? this.__canvas._nbmpRef : (this._image ? this._image._nbmpRef : null);
        if (!srcRef) return '#000000';
        return __rpg2lx.bmpGetPixelHex(srcRef, x | 0, y | 0);
    };

    Bitmap.prototype.getAlphaPixel = function(x, y) {
        var srcRef = this.__canvas ? this.__canvas._nbmpRef : (this._image ? this._image._nbmpRef : null);
        if (!srcRef) return 0;
        return __rpg2lx.bmpGetAlpha(srcRef, x | 0, y | 0);
    };

    Bitmap.prototype.adjustTone = function(r, g, b) {
        if ((r || g || b) && this.width > 0 && this.height > 0) {
            __rpg2lx.bmpAdjustTone(this._canvas._nbmpRef, r | 0, g | 0, b | 0);
            this._setDirty();
        }
    };

    Bitmap.prototype.rotateHue = function(offset) {
        if (offset && this.width > 0 && this.height > 0) {
            __rpg2lx.bmpRotateHue(this._canvas._nbmpRef, Number(offset) || 0);
            this._setDirty();
        }
    };

    Bitmap.prototype.blur = function() {
        if (this.width > 0 && this.height > 0) {
            __rpg2lx.bmpBlur(this._canvas._nbmpRef);
            this._setDirty();
        }
    };

    Bitmap.snap = function(stage) {
        var width = Graphics.width;
        var height = Graphics.height;
        var bitmap = new Bitmap(width, height);
        if (stage) {
            if (stage.updateTransform) stage.updateTransform();
            __rpg2lx.renderStageToBitmap(stage, bitmap._canvas._nbmpRef, width, height);
        }
        bitmap._setDirty();
        return bitmap;
    };

    // Native C++ PNG + .rpgmvp transparent decryption inside bmpLoadFile (even when Decrypter.hasEncryptedImages is true!)
    Bitmap.prototype._requestImage = function(url) {
        if (this._decodeAfterRequest && !this._loader) {
            this._loader = ResourceHandler.createLoader(url, this._requestImage.bind(this, url), this._onError.bind(this));
        }
        this._image = new Image();
        this._url = url;
        this._loadingState = 'requesting';
        this._image.src = url;
        this._image.addEventListener('load', this._loadListener = Bitmap.prototype._onLoad.bind(this));
        this._image.addEventListener('error', this._errorListener = this._loader || Bitmap.prototype._onError.bind(this));
    };

    // Helper for C++ renderer to obtain a Bitmap's active NativeBitmap ref in 1 property read
    Object.defineProperty(Bitmap.prototype, '_activeNativeRef', {
        get: function() {
            if (!this.isReady()) return null;
            if (this.__canvas) return this.__canvas._nbmpRef;
            if (this._image) return this._image._nbmpRef;
            return null;
        },
        configurable: true
    });

    // 2. Sprite tinting handled natively on GPU in C++ renderer (avoid expensive JS canvas tinter!)
    Sprite.prototype._needsTint = function() { return false; };

    // 3. Tilemap GPU acceleration: skip slow JS quarter-tile blitting and let C++ render lower/upper layers directly!
    var _Tilemap_createLayers = Tilemap.prototype._createLayers;
    Tilemap.prototype._createLayers = function() {
        this._isNativeTilemap = true;
        this._layerWidth = this._width;
        this._layerHeight = this._height;
        if (!this._lowerLayer) {
            this._lowerLayer = new Sprite();
            this._lowerLayer.z = 0;
            this._lowerLayer._nativeTilePass = 1;
            this._lowerLayer._parentTilemap = this;
        }
        if (!this._upperLayer) {
            this._upperLayer = new Sprite();
            this._upperLayer.z = 4;
            this._upperLayer._nativeTilePass = 2;
            this._upperLayer._parentTilemap = this;
        }
        if (this.children.indexOf(this._lowerLayer) < 0) this.addChild(this._lowerLayer);
        if (this.children.indexOf(this._upperLayer) < 0) this.addChild(this._upperLayer);
    };
    Tilemap.prototype._updateLayerPositions = function() {};
    Tilemap.prototype._paintAllTiles = function() {};
    Tilemap.prototype.refresh = function() {
        this._mapRevision = (this._mapRevision || 0) + 1;
    };
    var _Tilemap_setData = Tilemap.prototype.setData;
    Tilemap.prototype.setData = function(width, height, data) {
        _Tilemap_setData.call(this, width, height, data);
        this._mapRevision = (this._mapRevision || 0) + 1;
    };

    // 4. Graphics fast path (font preloaded in C++, Canvas mode, zero DOM manipulation overhead)
    Graphics.initialize = function(width, height, type) {
        this._width = width || 800;
        this._height = height || 600;
        this._boxWidth = this._width;
        this._boxHeight = this._height;
        this._scale = 1;
        this._realScale = 1;
        this._rendererType = 'canvas';
        this._loadingCount = 0;
        this._videoUnlocked = true;
        this.frameCount = 0;
        this._renderer = new PIXI.CanvasRenderer(this._width, this._height);
        __rpg2lx.onResize(this._width, this._height);
    };
    Object.defineProperty(Graphics, 'width', {
        get: function() { return this._width; },
        set: function(v) {
            if (this._width !== v) {
                this._width = v;
                if (this._renderer) this._renderer.resize(this._width, this._height);
                __rpg2lx.onResize(this._width, this._height);
            }
        },
        configurable: true
    });
    Object.defineProperty(Graphics, 'height', {
        get: function() { return this._height; },
        set: function(v) {
            if (this._height !== v) {
                this._height = v;
                if (this._renderer) this._renderer.resize(this._width, this._height);
                __rpg2lx.onResize(this._width, this._height);
            }
        },
        configurable: true
    });
    Graphics.isWebGL = function() { return false; };
    Graphics.hasWebGL = function() { return true; };
    Graphics.isFontLoaded = function() { return true; };
    Graphics.loadFont = function() {};
    Graphics.canUseCssFontLoading = function() { return true; };
    Graphics.canUseDifferenceBlend = function() { return true; };
    Graphics.canUseSaturationBlend = function() { return true; };
    Graphics.startLoading = function() { this._loadingCount = 0; };
    Graphics.updateLoading = function() { this._loadingCount++; };
    Graphics.endLoading = function() {};
    Graphics.printLoadingError = function(url) { __rpg2lx.reportError('Failed to load: ' + url); };
    Graphics.eraseLoadingError = function() {};
    Graphics.printError = function(name, message) { __rpg2lx.reportError(String(name) + ': ' + String(message)); };
    Graphics.tickStart = function() {};
    Graphics.tickEnd = function() {};
    Graphics.callGC = function() {};
    Graphics.playVideo = function() {};
    Graphics.isVideoPlaying = function() { return false; };
    Graphics.canPlayVideoType = function() { return false; };
    Graphics.setVideoVolume = function() {};
    Graphics.pageToCanvasX = function(x) { return x | 0; };
    Graphics.pageToCanvasY = function(y) { return y | 0; };
    Graphics.isInsideCanvas = function(x, y) { return x >= 0 && x < this._width && y >= 0 && y < this._height; };
    Graphics.render = function(stage) {
        if (stage) {
            this._renderer.render(stage);
        }
        this.frameCount++;
    };

    // 5. Window cursor reuse optimization (zero bitmap churn when navigating menus!)
    var _Window_refreshCursor = Window.prototype._refreshCursor;
    Window.prototype._refreshCursor = function() {
        var pad = this._padding;
        var x = this._cursorRect.x + pad - this.origin.x;
        var y = this._cursorRect.y + pad - this.origin.y;
        var w = this._cursorRect.width;
        var h = this._cursorRect.height;
        var m = 4;
        var x2 = Math.max(x, pad);
        var y2 = Math.max(y, pad);
        var ox = x - x2;
        var oy = y - y2;
        var w2 = Math.max(0, Math.min(w, this._width - pad - x2));
        var h2 = Math.max(0, Math.min(h, this._height - pad - y2));

        var bitmap = this._windowCursorSprite.bitmap;
        if (!bitmap) {
            bitmap = new Bitmap(Math.max(1, w2), Math.max(1, h2));
            this._windowCursorSprite.bitmap = bitmap;
        } else {
            if (bitmap.width !== Math.max(1, w2) || bitmap.height !== Math.max(1, h2)) {
                bitmap.resize(Math.max(1, w2), Math.max(1, h2));
            }
            bitmap.clear();
        }
        this._windowCursorSprite.setFrame(0, 0, w2, h2);
        this._windowCursorSprite.move(x2, y2);

        if (w2 > 0 && h2 > 0 && w > 0 && h > 0 && this._windowskin && this._windowskin.isReady()) {
            var skin = this._windowskin;
            var p = 96;
            var q = 48;
            bitmap.blt(skin, p+m, p+m, q-m*2, q-m*2, ox+m, oy+m, w-m*2, h-m*2);
            bitmap.blt(skin, p+m, p+0, q-m*2, m, ox+m, oy+0, w-m*2, m);
            bitmap.blt(skin, p+m, p+q-m, q-m*2, m, ox+m, oy+h-m, w-m*2, m);
            bitmap.blt(skin, p+0, p+m, m, q-m*2, ox+0, oy+m, m, h-m*2);
            bitmap.blt(skin, p+q-m, p+m, m, q-m*2, ox+w-m, oy+m, m, h-m*2);
            bitmap.blt(skin, p+0, p+0, m, m, ox+0, oy+0, m, m);
            bitmap.blt(skin, p+q-m, p+0, m, m, ox+w-m, oy+0, m, m);
            bitmap.blt(skin, p+0, p+q-m, m, m, ox+0, oy+h-m, m, m);
            bitmap.blt(skin, p+q-m, p+q-m, m, m, ox+w-m, oy+h-m, m, m);
        }
    };

    // 6. Native C++ WebAudio & Html5Audio (stb_vorbis + SDL2 mixer, supports .ogg & .rpgmvo + LOOPSTART/LOOPLENGTH!)
    WebAudio._initialized = true;
    WebAudio._canPlayOgg = true;
    WebAudio._canPlayM4a = false;
    WebAudio.initialize = function() { this._initialized = true; return true; };
    WebAudio.canPlayOgg = function() { return true; };
    WebAudio.canPlayM4a = function() { return false; };
    WebAudio.setMasterVolume = function(value) {
        this._masterVolume = Math.max(0, Math.min(1, Number(value) || 0));
        __rpg2lx.audioSetMasterVolume(this._masterVolume);
    };

    WebAudio.prototype.initialize = function(url) {
        this._url = String(url || '');
        this._volume = 1;
        this._pitch = 1;
        this._pan = 0;
        this._loadListeners = [];
        this._stopListeners = [];
        this._hasError = false;
        this._autoPlay = false;
        this._playerId = __rpg2lx.audioCreate(this._url);
        this._isReadyFlag = false;
        var self = this;
        __queueAsync(function() {
            if (__rpg2lx.audioIsReady(self._playerId)) {
                self._isReadyFlag = true;
                while (self._loadListeners.length > 0) {
                    var fn = self._loadListeners.shift();
                    if (typeof fn === 'function') fn();
                }
            } else {
                // Missing optional audio should not crash gameplay; mark ready/silent
                self._isReadyFlag = true;
                while (self._loadListeners.length > 0) {
                    var fn2 = self._loadListeners.shift();
                    if (typeof fn2 === 'function') fn2();
                }
            }
        });
    };
    WebAudio.prototype.clear = function() {
        this.stop();
        if (this._playerId) {
            __rpg2lx.audioDestroy(this._playerId);
            this._playerId = 0;
        }
        this._volume = 1;
        this._pitch = 1;
        this._pan = 0;
        this._loadListeners = [];
        this._stopListeners = [];
        this._isReadyFlag = false;
    };
    Object.defineProperty(WebAudio.prototype, 'volume', {
        get: function() { return this._volume; },
        set: function(v) {
            this._volume = Math.max(0, Math.min(1, Number(v) || 0));
            if (this._playerId) __rpg2lx.audioSetVolume(this._playerId, this._volume);
        },
        configurable: true
    });
    Object.defineProperty(WebAudio.prototype, 'pitch', {
        get: function() { return this._pitch; },
        set: function(v) {
            this._pitch = Math.max(0.25, Math.min(4.0, Number(v) || 1));
            if (this._playerId) __rpg2lx.audioSetPitch(this._playerId, this._pitch);
        },
        configurable: true
    });
    Object.defineProperty(WebAudio.prototype, 'pan', {
        get: function() { return this._pan; },
        set: function(v) {
            this._pan = Math.max(-1, Math.min(1, Number(v) || 0));
            if (this._playerId) __rpg2lx.audioSetPan(this._playerId, this._pan);
        },
        configurable: true
    });
    WebAudio.prototype.isReady = function() { return !!this._isReadyFlag; };
    WebAudio.prototype.isError = function() { return false; };
    WebAudio.prototype.isPlaying = function() { return this._playerId ? __rpg2lx.audioIsPlaying(this._playerId) : false; };
    WebAudio.prototype.play = function(loop, offset) {
        var self = this;
        if (this._isReadyFlag) {
            if (this._playerId) {
                __rpg2lx.audioSetVolume(this._playerId, this._volume);
                __rpg2lx.audioSetPitch(this._playerId, this._pitch);
                __rpg2lx.audioSetPan(this._playerId, this._pan);
                __rpg2lx.audioPlay(this._playerId, !!loop, Number(offset) || 0);
            }
        } else {
            this._autoPlay = true;
            this.addLoadListener(function() {
                if (self._autoPlay) self.play(loop, offset);
            });
        }
    };
    WebAudio.prototype.stop = function() {
        this._autoPlay = false;
        if (this._playerId) __rpg2lx.audioStop(this._playerId);
        if (this._stopListeners) {
            while (this._stopListeners.length > 0) {
                var fn = this._stopListeners.shift();
                if (typeof fn === 'function') fn();
            }
        }
    };
    WebAudio.prototype.fadeIn = function(duration) {
        var self = this;
        if (this._isReadyFlag) {
            if (this._playerId) __rpg2lx.audioFadeIn(this._playerId, Number(duration) || 0);
        } else if (this._autoPlay) {
            this.addLoadListener(function() { self.fadeIn(duration); });
        }
    };
    WebAudio.prototype.fadeOut = function(duration) {
        this._autoPlay = false;
        if (this._playerId) __rpg2lx.audioFadeOut(this._playerId, Number(duration) || 0);
    };
    WebAudio.prototype.seek = function() {
        return this._playerId ? __rpg2lx.audioSeek(this._playerId) : 0;
    };
    WebAudio.prototype.addLoadListener = function(listner) {
        if (this._isReadyFlag) listner();
        else this._loadListeners.push(listner);
    };
    WebAudio.prototype.addStopListener = function(listner) {
        this._stopListeners.push(listner);
    };

    Html5Audio.initialize = function() { this._initialized = true; return true; };
    Html5Audio.setup = function(url) {
        if (this._wa) this._wa.stop();
        this._wa = new WebAudio(url);
    };
    Html5Audio.isReady = function() { return this._wa ? this._wa.isReady() : true; };
    Html5Audio.play = function(loop, offset) { if (this._wa) this._wa.play(loop, offset); };
    Html5Audio.stop = function() { if (this._wa) this._wa.stop(); };
    Html5Audio.fadeIn = function(d) { if (this._wa) this._wa.fadeIn(d); };
    Html5Audio.fadeOut = function(d) { if (this._wa) this._wa.fadeOut(d); };
    Html5Audio.seek = function() { return this._wa ? this._wa.seek() : 0; };
})(globalThis);
)JS";

inline constexpr const char* kMvPostManagersPatchJs = R"JS(
(function(global) {
    // Isolate all .rpgsave files into RPG2LX's save_dir while keeping 100% MV .rpgsave compatibility
    StorageManager.isLocalMode = function() { return true; };
    StorageManager.localFileDirectoryPath = function() { return '/__rpg2lx_saves__/'; };

    // Always prefer our accelerated C++ renderer
    SceneManager.preferableRendererType = function() { return 'canvas'; };
    SceneManager.shouldUseCanvasRenderer = function() { return true; };
    SceneManager.initNwjs = function() {};
    SceneManager.terminate = function() { __rpg2lx.requestExit(); };

    // Deterministic 1:1 60 FPS frame driver (zero accumulator micro-stutter!)
    SceneManager.requestUpdate = function() {};
    global.__rpg2lxStepScene = function() {
        try {
            if (SceneManager._stopped) return;
            SceneManager.tickStart();
            SceneManager.updateManagers();
            SceneManager.updateInputData();
            SceneManager.changeScene();
            SceneManager.updateScene();
            SceneManager.renderScene();
            SceneManager.tickEnd();
        } catch (e) {
            __rpg2lx.reportError(e && e.stack ? String(e.stack) : String(e));
            SceneManager.stop();
        }
    };

    // Direct C++ input injection into MV Input & TouchInput
    global.__rpg2lxSetInput = function(name, pressed) {
        if (pressed) {
            if (ResourceHandler && ResourceHandler.exists && ResourceHandler.exists() && name === 'ok') {
                ResourceHandler.retry();
                return;
            }
            Input._currentState[name] = true;
        } else {
            Input._currentState[name] = false;
        }
    };
    global.__rpg2lxClearInput = function() {
        Input.clear();
        TouchInput.clear();
    };
    global.__rpg2lxPointerMove = function(x, y) {
        TouchInput._onMove(x | 0, y | 0);
    };
    global.__rpg2lxPointerDown = function(x, y, rightBtn) {
        if (rightBtn) TouchInput._onCancel(x | 0, y | 0);
        else {
            TouchInput._mousePressed = true;
            TouchInput._pressedTime = 0;
            TouchInput._onTrigger(x | 0, y | 0);
        }
    };
    global.__rpg2lxPointerUp = function(x, y) {
        TouchInput._mousePressed = false;
        TouchInput._onRelease(x | 0, y | 0);
    };

    // Ensure AudioManager uses .ogg (decoded natively via stb_vorbis, or .rpgmvo transparently)
    AudioManager.audioFileExt = function() { return '.ogg'; };
})(globalThis);
)JS";

} // namespace rpg2lx
