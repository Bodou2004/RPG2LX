#pragma once

namespace rpg2lx {

// Browser / NW.js surface that plugins rely on beyond what rpg_core.js itself needs.
// Evaluated right after kMvPreCoreBootstrapJs and before any game script. Everything here
// only fills gaps: an API that already exists is never replaced.
inline constexpr const char* kMvBrowserCompatJs = R"JS(
(function(global) {
    function def(name, value) { if (typeof global[name] === 'undefined') global[name] = value; }

    // "TypeError: x is undefined" plus the stack; QuickJS stacks omit the message line.
    global.__errorText = function(e) {
        if (e && typeof e === 'object') {
            var head = (e.name || 'Error') + ': ' + (e.message !== undefined ? e.message : '');
            var stack = e.stack ? String(e.stack) : '';
            return stack.indexOf(head) === 0 ? stack : head + (stack ? '\n' + stack : '');
        }
        return String(e);
    };

    // ---- Events ---------------------------------------------------------------------------
    function Event(type, init) {
        init = init || {};
        this.type = String(type);
        this.bubbles = !!init.bubbles;
        this.cancelable = !!init.cancelable;
        this.composed = !!init.composed;
        this.defaultPrevented = false;
        this.isTrusted = false;
        this.target = null;
        this.currentTarget = null;
        this.timeStamp = __rpg2lx.nowMs();
        this._stopImmediate = false;
        for (var k in init) if (!(k in this)) this[k] = init[k];
    }
    Event.prototype.preventDefault = function() { this.defaultPrevented = true; };
    Event.prototype.stopPropagation = function() {};
    Event.prototype.stopImmediatePropagation = function() { this._stopImmediate = true; };
    function subEvent(name) {
        var C = function(type, init) { Event.call(this, type, init); };
        C.prototype = Object.create(Event.prototype);
        C.prototype.constructor = C;
        Object.defineProperty(C, 'name', { value: name });
        return C;
    }
    def('Event', Event);
    def('CustomEvent', (function() {
        var C = subEvent('CustomEvent');
        var Base = C;
        C = function(type, init) { Base.call(this, type, init); this.detail = init && 'detail' in init ? init.detail : null; };
        C.prototype = Base.prototype;
        return C;
    })());
    ['UIEvent', 'KeyboardEvent', 'MouseEvent', 'PointerEvent', 'WheelEvent', 'TouchEvent', 'FocusEvent',
     'InputEvent', 'ErrorEvent', 'ProgressEvent', 'MessageEvent', 'GamepadEvent'].forEach(function(n) { def(n, subEvent(n)); });
    def('EventTarget', function EventTarget() { global.__makeEventTarget(this); });

    // ---- DOM ------------------------------------------------------------------------------
    function Node() {}
    Node.ELEMENT_NODE = 1; Node.TEXT_NODE = 3; Node.DOCUMENT_NODE = 9;
    function Element() {}
    Element.prototype = Object.create(Node.prototype);
    function HTMLElement() {}
    HTMLElement.prototype = Object.create(Element.prototype);
    var tagClasses = {};
    ['Canvas', 'Image', 'Video', 'Audio', 'Media', 'Div', 'Span', 'Script', 'Style', 'Link', 'Input', 'Button',
     'Body', 'Head', 'Html', 'Paragraph', 'Anchor', 'TextArea', 'Select', 'Option', 'Label', 'IFrame', 'Unknown']
        .forEach(function(n) {
            var C = function() {};
            C.prototype = Object.create(HTMLElement.prototype);
            Object.defineProperty(C, 'name', { value: 'HTML' + n + 'Element' });
            tagClasses[n] = C;
            def('HTML' + n + 'Element', C);
        });
    tagClasses.Video.prototype = Object.create(tagClasses.Media.prototype);
    tagClasses.Audio.prototype = Object.create(tagClasses.Media.prototype);
    def('Node', Node); def('Element', Element); def('HTMLElement', HTMLElement);
    def('Document', function Document() {});
    def('Text', function Text() {});
    def('DocumentFragment', function DocumentFragment() {});
    var tagToClass = { CANVAS: 'Canvas', IMG: 'Image', VIDEO: 'Video', AUDIO: 'Audio', DIV: 'Div', SPAN: 'Span',
        SCRIPT: 'Script', STYLE: 'Style', LINK: 'Link', INPUT: 'Input', BUTTON: 'Button', BODY: 'Body', HEAD: 'Head',
        HTML: 'Html', P: 'Paragraph', A: 'Anchor', TEXTAREA: 'TextArea', SELECT: 'Select', OPTION: 'Option',
        LABEL: 'Label', IFRAME: 'IFrame' };

    function classList(el) {
        function list() { return String(el.className || '').split(/\s+/).filter(Boolean); }
        function save(items) { el.className = items.join(' '); }
        return {
            add: function() { var l = list(); for (var i = 0; i < arguments.length; i++) if (l.indexOf(arguments[i]) < 0) l.push(arguments[i]); save(l); },
            remove: function() { var a = Array.prototype.slice.call(arguments); save(list().filter(function(c) { return a.indexOf(c) < 0; })); },
            contains: function(c) { return list().indexOf(c) >= 0; },
            toggle: function(c, force) {
                var has = list().indexOf(c) >= 0, want = force === undefined ? !has : !!force;
                if (want && !has) this.add(c); else if (!want && has) this.remove(c);
                return want;
            },
            get length() { return list().length; },
            item: function(i) { return list()[i] || null; }
        };
    }

    function styleObject() {
        var s = {};
        Object.defineProperty(s, 'setProperty', { value: function(k, v) { this[k] = v; } });
        Object.defineProperty(s, 'getPropertyValue', { value: function(k) { return this[k] !== undefined ? String(this[k]) : ''; } });
        Object.defineProperty(s, 'removeProperty', { value: function(k) { var v = this[k]; delete this[k]; return v; } });
        return s;
    }

    function matches(el, selector) {
        selector = String(selector).trim();
        if (!el || el.nodeType !== 1) return false;
        if (selector === '*') return true;
        var m = selector.match(/^([a-zA-Z0-9-]*)(#[\w-]+)?((?:\.[\w-]+)*)(\[[^\]]+\])?$/);
        if (!m) return false;
        if (m[1] && el.tagName !== m[1].toUpperCase()) return false;
        if (m[2] && el.id !== m[2].slice(1)) return false;
        if (m[3]) {
            var cls = m[3].split('.').filter(Boolean);
            for (var i = 0; i < cls.length; i++) if (!el.classList.contains(cls[i])) return false;
        }
        if (m[4]) {
            var a = m[4].slice(1, -1).split('=');
            var val = el.getAttribute(a[0]);
            if (val === null) return false;
            if (a.length > 1 && val !== a[1].replace(/^["']|["']$/g, '')) return false;
        }
        return true;
    }
    function walk(root, fn) {
        var kids = root.children || [];
        for (var i = 0; i < kids.length; i++) {
            if (fn(kids[i]) === true) return true;
            if (walk(kids[i], fn)) return true;
        }
        return false;
    }
    function query(root, selector, all) {
        var parts = String(selector).split(',');
        var out = [];
        walk(root, function(el) {
            for (var i = 0; i < parts.length; i++) {
                // Only the last compound of a descendant selector is checked.
                var last = parts[i].trim().split(/\s+/).pop();
                if (matches(el, last)) { out.push(el); return !all; }
            }
        });
        return all ? out : (out[0] || null);
    }

    var EP = HTMLElement.prototype;
    EP.nodeType = 1;
    EP.setAttribute = function(k, v) { this._attrs[k] = String(v); if (k === 'id') this.id = String(v); if (k === 'class') this.className = String(v); };
    EP.getAttribute = function(k) {
        if (k === 'id') return this.id || null;
        if (k === 'class') return this.className || null;
        return Object.prototype.hasOwnProperty.call(this._attrs, k) ? this._attrs[k] : null;
    };
    EP.hasAttribute = function(k) { return this.getAttribute(k) !== null; };
    EP.removeAttribute = function(k) { delete this._attrs[k]; };
    EP.appendChild = function(child) {
        if (child && child.nodeType === 11) { while (child.children.length) this.appendChild(child.children[0]); return child; }
        if (child && child.parentNode && child.parentNode !== this && child.parentNode.removeChild) child.parentNode.removeChild(child);
        if (this.children.indexOf(child) < 0) this.children.push(child);
        if (child) child.parentNode = this;
        return child;
    };
    EP.append = function() { for (var i = 0; i < arguments.length; i++) this.appendChild(arguments[i]); };
    EP.prepend = function(child) { this.insertBefore(child, this.children[0] || null); };
    EP.insertBefore = function(child, ref) {
        this.appendChild(child);
        var i = this.children.indexOf(child), r = ref ? this.children.indexOf(ref) : -1;
        if (r >= 0 && i >= 0) { this.children.splice(i, 1); this.children.splice(r, 0, child); }
        return child;
    };
    EP.removeChild = function(child) {
        var i = this.children.indexOf(child);
        if (i >= 0) this.children.splice(i, 1);
        if (child) child.parentNode = null;
        return child;
    };
    EP.replaceChild = function(next, old) { this.insertBefore(next, old); return this.removeChild(old); };
    EP.remove = function() { if (this.parentNode) this.parentNode.removeChild(this); };
    EP.contains = function(other) { return other === this || walk(this, function(el) { return el === other; }); };
    EP.hasChildNodes = function() { return this.children.length > 0; };
    EP.cloneNode = function() { var c = global.document.createElement(this.tagName); c.className = this.className; c.id = this.id; return c; };
    EP.querySelector = function(s) { return query(this, s, false); };
    EP.querySelectorAll = function(s) { return query(this, s, true); };
    EP.getElementsByTagName = function(t) { return query(this, String(t).toLowerCase() === '*' ? '*' : t, true); };
    EP.getElementsByClassName = function(c) { return query(this, '.' + String(c).trim().split(/\s+/).join('.'), true); };
    EP.matches = function(s) { return String(s).split(',').some(function(p) { return matches(this, p.trim()); }, this); };
    EP.closest = function(s) { for (var el = this; el; el = el.parentNode) if (el.matches && el.matches(s)) return el; return null; };
    EP.getBoundingClientRect = function() {
        var w = this.width || 0, h = this.height || 0;
        return { x: 0, y: 0, left: 0, top: 0, right: w, bottom: h, width: w, height: h };
    };
    EP.getClientRects = function() { return [this.getBoundingClientRect()]; };
    EP.focus = function() { global.document.activeElement = this; };
    EP.blur = function() { if (global.document.activeElement === this) global.document.activeElement = global.document.body; };
    EP.click = function() { this.dispatchEvent(new (global.MouseEvent)('click', { bubbles: true })); };
    EP.scrollIntoView = function() {};
    EP.requestFullscreen = function() { return Promise.resolve(); };
    EP.webkitRequestFullScreen = EP.mozRequestFullScreen = EP.msRequestFullscreen = function() {};
    EP.animate = function() { return { finished: Promise.resolve(), cancel: function() {}, play: function() {}, pause: function() {} }; };
    ['addEventListener', 'removeEventListener', 'dispatchEvent'].forEach(function(name) {
        EP[name] = function() {
            if (!this.__events) Object.defineProperty(this, '__events', { value: global.__makeEventTarget({}) });
            return this.__events[name].apply(this.__events, arguments);
        };
    });
    Object.defineProperty(EP, 'childNodes', { get: function() { return this.children; } });
    Object.defineProperty(EP, 'firstChild', { get: function() { return this.children[0] || null; } });
    Object.defineProperty(EP, 'lastChild', { get: function() { return this.children[this.children.length - 1] || null; } });
    Object.defineProperty(EP, 'firstElementChild', { get: function() { return this.children[0] || null; } });
    Object.defineProperty(EP, 'childElementCount', { get: function() { return this.children.length; } });
    Object.defineProperty(EP, 'parentElement', { get: function() { return this.parentNode || null; } });
    Object.defineProperty(EP, 'offsetWidth', { get: function() { return this.width || global.innerWidth; } });
    Object.defineProperty(EP, 'offsetHeight', { get: function() { return this.height || global.innerHeight; } });
    Object.defineProperty(EP, 'clientWidth', { get: function() { return this.width || global.innerWidth; } });
    Object.defineProperty(EP, 'clientHeight', { get: function() { return this.height || global.innerHeight; } });
    // Media elements: video/audio files are not decoded; they report an error so MV skips them.
    var MP = tagClasses.Media.prototype;
    MP.play = function() {
        var self = this;
        __queueAsync(function() {
            var e = new Event('error');
            if (typeof self.onerror === 'function') self.onerror(e);
            self.dispatchEvent(e);
        });
        return Promise.resolve();
    };
    MP.pause = function() {}; MP.load = function() {};
    MP.canPlayType = function(mime) { return String(mime).indexOf('ogg') >= 0 && this.tagName === 'AUDIO' ? 'probably' : ''; };
    MP.paused = true; MP.ended = false; MP.currentTime = 0; MP.duration = 0; MP.volume = 1; MP.muted = false; MP.readyState = 0;

    // Gives a shim element (old makeElement objects, native canvases) the full element API.
    global.__upgradeElement = function(el) {
        if (!el || el.__upgraded) return el;
        var cls = tagClasses[tagToClass[String(el.tagName || '').toUpperCase()] || 'Unknown'];
        // The original shim's no-op methods would shadow the working ones on the prototype.
        ['addEventListener', 'removeEventListener', 'canPlayType', 'requestFullscreen'].forEach(function(k) { delete el[k]; });
        if (el.tagName !== 'CANVAS') delete el.getContext;
        Object.setPrototypeOf(el, cls.prototype);
        Object.defineProperty(el, '__upgraded', { value: true });
        if (!el.style || typeof el.style.setProperty !== 'function') {
            var old = el.style || {}, s = styleObject();
            for (var k in old) s[k] = old[k];
            el.style = s;
        }
        if (!el.children) el.children = [];
        if (!el._attrs) Object.defineProperty(el, '_attrs', { value: {}, writable: true });
        if (el.id === undefined) el.id = '';
        if (el.className === undefined) el.className = '';
        if (!el.dataset) el.dataset = {};
        if (el.parentNode === undefined) el.parentNode = null;
        el.ownerDocument = global.document;
        if (!el.classList) Object.defineProperty(el, 'classList', { value: classList(el) });
        return el;
    };

    var doc = global.document;
    var createElement = doc.createElement;
    doc.createElement = function(tag) { return global.__upgradeElement(createElement.call(doc, tag)); };
    doc.createElementNS = function(ns, tag) { return doc.createElement(tag); };
    doc.createTextNode = function(text) { return { nodeType: 3, textContent: String(text), data: String(text), parentNode: null }; };
    doc.createDocumentFragment = function() { var f = doc.createElement('div'); f.nodeType = 11; return f; };
    doc.createComment = function() { return { nodeType: 8 }; };
    [doc.head, doc.body, doc.documentElement].forEach(global.__upgradeElement);
    doc.documentElement.appendChild(doc.head);
    doc.documentElement.appendChild(doc.body);
    var nativeCanvas = global.__createNativeCanvas;
    global.__createNativeCanvas = function() { return global.__upgradeElement(nativeCanvas.apply(this, arguments)); };
    Object.setPrototypeOf(global.Image.prototype, tagClasses.Image.prototype);
    def('Audio', function Audio(src) { var a = doc.createElement('audio'); if (src) a.src = src; return a; });

    doc.nodeType = 9;
    doc.readyState = 'complete';
    doc.hidden = false;
    doc.activeElement = doc.body;
    doc.fullscreenElement = null;
    doc.fullscreenEnabled = false;
    doc.cookie = '';
    doc.referrer = '';
    doc.URL = global.location.href;
    doc.characterSet = 'UTF-8';
    doc.defaultView = global;
    doc.hasFocus = function() { return true; };
    doc.exitFullscreen = function() { return Promise.resolve(); };
    doc.webkitCancelFullScreen = doc.mozCancelFullScreen = doc.msExitFullscreen = function() {};
    doc.getElementById = function(id) { return query(doc.documentElement, '#' + id, false); };
    doc.querySelector = function(s) { return query(doc.documentElement, s, false); };
    doc.querySelectorAll = function(s) { return query(doc.documentElement, s, true); };
    doc.getElementsByClassName = function(c) { return doc.documentElement.getElementsByClassName(c); };
    doc.getElementsByName = function(n) { return query(doc.documentElement, '[name=' + n + ']', true); };
    var byTag = doc.getElementsByTagName;
    doc.getElementsByTagName = function(t) {
        var found = query(doc.documentElement, t, true);
        return found.length ? found : byTag.call(doc, t);
    };
    doc.elementFromPoint = function() { return doc.body; };
    doc.execCommand = function() { return false; };

    // ---- Window ---------------------------------------------------------------------------
    global.focus = function() {};
    global.blur = function() {};
    global.scrollTo = global.scroll = global.scrollBy = function() {};
    global.open = function() { return null; };
    global.print = function() {};
    global.confirm = function() { return true; };
    global.prompt = function(msg, value) { return value === undefined ? '' : value; };
    def('devicePixelRatio', 1);
    def('screen', { width: 816, height: 624, availWidth: 816, availHeight: 624, colorDepth: 24, pixelDepth: 24,
                    orientation: { type: 'landscape-primary', angle: 0, addEventListener: function() {} } });
    def('screenX', 0); def('screenY', 0); def('outerWidth', global.innerWidth); def('outerHeight', global.innerHeight);
    def('scrollX', 0); def('scrollY', 0);
    def('parent', global); def('frames', global); def('opener', null); def('name', '');
    def('history', { length: 1, state: null, pushState: function() {}, replaceState: function() {}, back: function() {}, forward: function() {}, go: function() {} });
    global.getComputedStyle = function(el) {
        var s = (el && el.style) || {};
        return new Proxy(s, { get: function(t, k) {
            if (k === 'getPropertyValue') return function(p) { return t[p] !== undefined ? String(t[p]) : ''; };
            return t[k] !== undefined ? t[k] : '';
        } });
    };
    global.matchMedia = function(q) {
        return { matches: false, media: String(q), onchange: null, addListener: function() {}, removeListener: function() {},
                 addEventListener: function() {}, removeEventListener: function() {} };
    };
    global.location.protocol = 'file:';
    global.location.host = global.location.hostname = global.location.port = '';
    global.location.pathname = '/game/index.html';
    global.location.origin = 'file://';
    global.location.assign = global.location.replace = function() {};
    global.location.toString = function() { return this.href; };
    global.navigator.platform = 'Linux aarch64';
    global.navigator.languages = [global.navigator.language, 'en-US'];
    global.navigator.maxTouchPoints = 0;
    global.navigator.hardwareConcurrency = 4;
    global.navigator.cookieEnabled = false;
    global.navigator.vibrate = function() { return false; };
    global.navigator.sendBeacon = function() { return false; };
    global.navigator.clipboard = { writeText: function() { return Promise.resolve(); }, readText: function() { return Promise.resolve(''); } };

    // ---- Encoding / binary helpers QuickJS lacks ----------------------------------------
    var B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    def('btoa', function(input) {
        var s = String(input), out = '';
        for (var i = 0; i < s.length; i += 3) {
            var a = s.charCodeAt(i), b = s.charCodeAt(i + 1), c = s.charCodeAt(i + 2);
            if (a > 255 || b > 255 || c > 255) throw new Error('InvalidCharacterError: btoa');
            var n = (a << 16) | ((b || 0) << 8) | (c || 0);
            out += B64[(n >> 18) & 63] + B64[(n >> 12) & 63] + (i + 1 < s.length ? B64[(n >> 6) & 63] : '=') + (i + 2 < s.length ? B64[n & 63] : '=');
        }
        return out;
    });
    def('atob', function(input) {
        var s = String(input).replace(/[\s=]/g, ''), out = '', buffer = 0, bits = 0;
        for (var i = 0; i < s.length; i++) {
            var v = B64.indexOf(s[i]);
            if (v < 0) throw new Error('InvalidCharacterError: atob');
            buffer = (buffer << 6) | v; bits += 6;
            if (bits >= 8) { bits -= 8; out += String.fromCharCode((buffer >> bits) & 255); }
        }
        return out;
    });
    def('TextEncoder', function TextEncoder() {
        this.encoding = 'utf-8';
        this.encode = function(str) {
            var utf8 = unescape(encodeURIComponent(String(str === undefined ? '' : str)));
            var out = new Uint8Array(utf8.length);
            for (var i = 0; i < utf8.length; i++) out[i] = utf8.charCodeAt(i);
            return out;
        };
    });
    def('TextDecoder', function TextDecoder(label) {
        this.encoding = label || 'utf-8';
        this.decode = function(buf) {
            if (!buf) return '';
            var bytes = buf instanceof Uint8Array ? buf : new Uint8Array(buf.buffer || buf, buf.byteOffset || 0, buf.byteLength);
            var s = '';
            for (var i = 0; i < bytes.length; i += 8192) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 8192));
            try { return decodeURIComponent(escape(s)); } catch (e) { return s; }
        };
    });
    def('crypto', {
        // Bytes from the system's secure random source, as in a browser.
        getRandomValues: function(arr) {
            if (!arr || !ArrayBuffer.isView(arr)) throw new TypeError('getRandomValues needs an integer typed array');
            var src = new Uint8Array(__rpg2lx.randomBytes(arr.byteLength));
            new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength).set(src);
            return arr;
        },
        randomUUID: function() {
            var b = new Uint8Array(__rpg2lx.randomBytes(16)), h = [];
            b[6] = (b[6] & 15) | 64; b[8] = (b[8] & 63) | 128;
            for (var i = 0; i < 16; i++) h.push((b[i] < 16 ? '0' : '') + b[i].toString(16));
            return h.slice(0, 4).join('') + '-' + h.slice(4, 6).join('') + '-' + h.slice(6, 8).join('') + '-' +
                   h.slice(8, 10).join('') + '-' + h.slice(10).join('');
        }
    });
    def('queueMicrotask', function(fn) { Promise.resolve().then(fn); });
    def('structuredClone', function(v) { return v === undefined ? undefined : JSON.parse(JSON.stringify(v)); });
    def('setImmediate', function(fn) { var a = Array.prototype.slice.call(arguments, 1); return setTimeout(function() { fn.apply(null, a); }, 0); });
    def('clearImmediate', function(id) { clearTimeout(id); });
    def('requestIdleCallback', function(fn) { return setTimeout(function() { fn({ didTimeout: false, timeRemaining: function() { return 8; } }); }, 1); });
    def('cancelIdleCallback', function(id) { clearTimeout(id); });

    // ---- Storage --------------------------------------------------------------------------
    function memoryStorage() {
        var map = {};
        return {
            getItem: function(k) { return Object.prototype.hasOwnProperty.call(map, k) ? map[k] : null; },
            setItem: function(k, v) { map[String(k)] = String(v); },
            removeItem: function(k) { delete map[String(k)]; },
            clear: function() { map = {}; },
            key: function(i) { return Object.keys(map)[i] || null; },
            get length() { return Object.keys(map).length; }
        };
    }
    def('sessionStorage', memoryStorage());

    // ---- Blob / FileReader / fetch -------------------------------------------------------
    var OldBlob = global.Blob;
    global.Blob = function Blob(parts, options) {
        OldBlob.call(this, parts);
        this.type = (options && options.type) || '';
        var text = (parts || []).map(function(p) { return typeof p === 'string' ? p : (p && p.byteLength !== undefined ? new TextDecoder().decode(p) : String(p)); }).join('');
        this._text = text;
        this.size = text.length;
    };
    global.Blob.prototype.text = function() { return Promise.resolve(this._text); };
    global.Blob.prototype.arrayBuffer = function() { return Promise.resolve(new TextEncoder().encode(this._text).buffer); };
    global.Blob.prototype.slice = function(a, b) { return new global.Blob([this._text.slice(a, b)], { type: this.type }); };
    def('File', function File(parts, name, options) { global.Blob.call(this, parts, options); this.name = name; this.lastModified = Date.now(); });
    def('FileReader', function FileReader() {
        var self = this;
        global.__makeEventTarget(this);
        this.readyState = 0; this.result = null; this.error = null;
        function finish(result) {
            __queueAsync(function() {
                self.readyState = 2; self.result = result;
                var e = new Event('load');
                if (typeof self.onload === 'function') self.onload(e);
                self.dispatchEvent(e);
                if (typeof self.onloadend === 'function') self.onloadend(e);
            });
        }
        this.readAsText = function(blob) { finish(blob && blob._text !== undefined ? blob._text : String(blob)); };
        this.readAsDataURL = function(blob) { finish('data:' + (blob.type || 'text/plain') + ';base64,' + btoa(unescape(encodeURIComponent(blob._text || '')))); };
        this.readAsArrayBuffer = function(blob) { finish(new TextEncoder().encode(blob._text || '').buffer); };
        this.abort = function() {};
    });
    function Response(body, status) {
        this.status = status; this.ok = status >= 200 && status < 300; this.statusText = this.ok ? 'OK' : 'Not Found';
        this._body = body;
        this.headers = { get: function() { return null; }, has: function() { return false; } };
    }
    Response.prototype.arrayBuffer = function() { return Promise.resolve(this._body || new ArrayBuffer(0)); };
    Response.prototype.text = function() { return Promise.resolve(this._body ? new TextDecoder().decode(this._body) : ''); };
    Response.prototype.json = function() { return this.text().then(JSON.parse); };
    Response.prototype.blob = function() { return this.text().then(function(t) { return new global.Blob([t]); }); };
    def('Response', Response);
    def('Headers', function Headers() { this.get = function() { return null; }; this.set = function() {}; this.has = function() { return false; }; });
    def('fetch', function(url) {
        url = typeof url === 'string' ? url : (url && url.url) || String(url);
        return new Promise(function(resolve, reject) {
            __queueAsync(function() {
                if (/^(https?:)?\/\//i.test(url)) { reject(new TypeError('Failed to fetch')); return; }
                var bytes = __rpg2lx.readProjectBytes(url);
                resolve(new Response(bytes, bytes === null || bytes === undefined ? 404 : 200));
            });
        });
    });
    def('AbortController', function AbortController() { this.signal = global.__makeEventTarget({ aborted: false }); this.abort = function() { this.signal.aborted = true; }; });

    // ---- Observers, workers, misc ---------------------------------------------------------
    function Observer() { this.observe = function() {}; this.unobserve = function() {}; this.disconnect = function() {}; this.takeRecords = function() { return []; }; }
    def('MutationObserver', Observer);
    def('ResizeObserver', Observer);
    def('IntersectionObserver', Observer);
    def('PerformanceObserver', Observer);
    def('Worker', function Worker() { global.__makeEventTarget(this); this.postMessage = function() {}; this.terminate = function() {}; });
    def('WebSocket', function WebSocket() { throw new Error('WebSocket is not available offline'); });
    def('Notification', { permission: 'denied', requestPermission: function() { return Promise.resolve('denied'); } });
    def('DOMParser', function DOMParser() { this.parseFromString = function() { return global.document; }; });
    def('XMLSerializer', function XMLSerializer() { this.serializeToString = function() { return ''; }; });
    if (!global.performance.mark) {
        global.performance.mark = global.performance.measure = global.performance.clearMarks = function() {};
        global.performance.getEntriesByName = global.performance.getEntriesByType = function() { return []; };
        global.performance.timeOrigin = Date.now();
        global.performance.memory = { usedJSHeapSize: 0, totalJSHeapSize: 0, jsHeapSizeLimit: 512 * 1024 * 1024 };
    }
    if (!global.console.debug) {
        ['debug', 'trace', 'table', 'dir', 'group', 'groupCollapsed', 'groupEnd', 'time', 'timeEnd', 'timeLog', 'count', 'assert', 'clear']
            .forEach(function(k) { global.console[k] = function() {}; });
    }
    global.onerror = null;
    global.onunhandledrejection = null;

    // ---- PIXI surface beyond what rpg_core.js needs ----------------------------------------
    // WebGL shaders cannot run here: filters keep their API (uniforms, padding, enabled) so a
    // plugin can create and tune them, and are simply not drawn.
    var PIXI = global.PIXI;
    function Filter(vertex, fragment, uniforms) {
        this.vertexSrc = vertex || ''; this.fragmentSrc = fragment || '';
        this.uniforms = {};
        this.uniformData = uniforms || {};
        for (var k in this.uniformData) {
            var u = this.uniformData[k];
            this.uniforms[k] = u && typeof u === 'object' && 'value' in u ? u.value : u;
        }
        this.enabled = true; this.padding = 0; this.resolution = 1; this.blendMode = 0; this.autoFit = true;
    }
    Filter.prototype.apply = function(filterManager, input, output, clear) {};
    Filter.prototype.destroy = function() {};
    Filter.defaultVertexSrc = Filter.defaultFragmentSrc = '';
    function subFilter(init) {
        var C = function() { Filter.call(this); if (init) init.apply(this, arguments); };
        C.prototype = Object.create(Filter.prototype);
        C.prototype.constructor = C;
        return C;
    }
    var BaseColorMatrix = PIXI.filters.ColorMatrixFilter;
    PIXI.Filter = Filter;
    PIXI.AbstractFilter = Filter;
    var oldFilters = PIXI.filters;
    PIXI.filters = {
        Filter: Filter,
        VoidFilter: subFilter(),
        AlphaFilter: subFilter(function(a) { this.alpha = a === undefined ? 1 : a; }),
        BlurFilter: subFilter(function(s) { this.blur = s || 8; this.quality = 4; this.blurX = this.blurY = this.blur; }),
        BlurXFilter: subFilter(function(s) { this.blur = s || 8; }),
        BlurYFilter: subFilter(function(s) { this.blur = s || 8; }),
        NoiseFilter: subFilter(function(n) { this.noise = n || 0.5; this.seed = Math.random(); }),
        DisplacementFilter: subFilter(function(sprite, scale) { this.maskSprite = sprite; this.scale = new PIXI.Point(scale || 20, scale || 20); }),
        FXAAFilter: subFilter(),
        ColorMatrixFilter: BaseColorMatrix
    };
    ['brightness', 'greyscale', 'grayscale', 'blackAndWhite', 'contrast', 'sepia', 'negative', 'desaturate', 'night',
     'predator', 'lsd', 'technicolor', 'polaroid', 'toBGR', 'kodachrome', 'browni', 'vintage', 'colorTone', 'tint']
        .forEach(function(n) { if (!BaseColorMatrix.prototype[n]) BaseColorMatrix.prototype[n] = function() {}; });
    for (var fk in oldFilters) if (!PIXI.filters[fk]) PIXI.filters[fk] = oldFilters[fk];
    PIXI.Shader = function Shader() { this.uniforms = {}; };
    PIXI.Program = { from: function() { return {}; } };
    PIXI.Geometry = function Geometry() {};
    PIXI.ObservablePoint = PIXI.Point;
    // pixi.js v4 still exposes these v2/v3 names through its deprecation layer, and older plugins use them.
    var oldNames = { DisplayObjectContainer: PIXI.Container, SpriteBatch: PIXI.Container, AbstractFilter: PIXI.Filter,
                     TilingSprite: PIXI.extras && PIXI.extras.TilingSprite, MovieClip: PIXI.extras && PIXI.extras.AnimatedSprite,
                     BitmapText: PIXI.extras && PIXI.extras.BitmapText, TransformManual: PIXI.TransformBase };
    for (var on in oldNames) if (oldNames[on] && !PIXI[on]) PIXI[on] = oldNames[on];
    PIXI.DEG_TO_RAD = Math.PI / 180;
    PIXI.RAD_TO_DEG = 180 / Math.PI;
    PIXI.PI_2 = Math.PI * 2;
    PIXI.WRAP_MODES = { CLAMP: 0, REPEAT: 1, MIRRORED_REPEAT: 2 };
    PIXI.TEXT_GRADIENT = { LINEAR_VERTICAL: 0, LINEAR_HORIZONTAL: 1 };
    PIXI.utils.hex2rgb = function(hex, out) { out = out || []; out[0] = ((hex >> 16) & 255) / 255; out[1] = ((hex >> 8) & 255) / 255; out[2] = (hex & 255) / 255; return out; };
    PIXI.utils.rgb2hex = function(rgb) { return ((rgb[0] * 255) << 16) + ((rgb[1] * 255) << 8) + (rgb[2] * 255 | 0); };
    PIXI.utils.hex2string = function(hex) { return '#' + ('00000' + (hex | 0).toString(16)).substr(-6); };
    PIXI.utils.string2hex = function(s) { return parseInt(String(s).replace('#', ''), 16); };
    PIXI.utils.EventEmitter = function EventEmitter() {};
    PIXI.utils.TextureCache = {};
    PIXI.utils.BaseTextureCache = {};
    PIXI.utils.isWebGLSupported = function() { return false; };
    var Matrix = PIXI.Matrix;
    if (!Matrix.prototype.apply) {
        Matrix.prototype.set = function(a, b, c, d, tx, ty) { this.a = a; this.b = b; this.c = c; this.d = d; this.tx = tx; this.ty = ty; return this; };
        Matrix.prototype.apply = function(p, out) { out = out || new PIXI.Point(); var x = p.x, y = p.y; out.x = this.a * x + this.c * y + this.tx; out.y = this.b * x + this.d * y + this.ty; return out; };
        Matrix.prototype.translate = function(x, y) { this.tx += x; this.ty += y; return this; };
        Matrix.prototype.scale = function(x, y) { this.a *= x; this.d *= y; this.c *= x; this.b *= y; this.tx *= x; this.ty *= y; return this; };
        Matrix.prototype.rotate = function(r) {
            var c = Math.cos(r), s = Math.sin(r), a = this.a, cc = this.c, tx = this.tx;
            this.a = a * c - this.b * s; this.b = a * s + this.b * c; this.c = cc * c - this.d * s; this.d = cc * s + this.d * c;
            this.tx = tx * c - this.ty * s; this.ty = tx * s + this.ty * c; return this;
        };
        Matrix.prototype.clone = function() { return new Matrix().set(this.a, this.b, this.c, this.d, this.tx, this.ty); };
        Matrix.prototype.copy = function(m) { return m.set(this.a, this.b, this.c, this.d, this.tx, this.ty); };
        Matrix.prototype.invert = function() {
            var a = this.a, b = this.b, c = this.c, d = this.d, tx = this.tx, n = a * d - b * c;
            this.a = d / n; this.b = -b / n; this.c = -c / n; this.d = a / n;
            this.tx = (c * this.ty - d * tx) / n; this.ty = -(a * this.ty - b * tx) / n; return this;
        };
        Matrix.IDENTITY = new Matrix();
        Matrix.TEMP_MATRIX = new Matrix();
    }
    var Point = PIXI.Point;
    if (!Point.prototype.equals) Point.prototype.equals = function(p) { return p.x === this.x && p.y === this.y; };
    // Graphics: every drawing call is accepted and chainable; filled rectangles keep working
    // through the existing native path.
    var G = PIXI.Graphics.prototype;
    ['lineStyle', 'moveTo', 'lineTo', 'quadraticCurveTo', 'bezierCurveTo', 'arc', 'arcTo', 'drawCircle', 'drawEllipse',
     'drawRoundedRect', 'drawPolygon', 'drawStar', 'closePath', 'beginTextureFill', 'lineTextureStyle', 'drawShape']
        .forEach(function(n) { if (!G[n]) G[n] = function() { return this; }; });
    if (!G.generateCanvasTexture) G.generateCanvasTexture = function() { return new PIXI.Texture(); };
    if (!G.getBounds) G.getBounds = function() { var s = this.graphicsData[0].shape; return new PIXI.Rectangle(s.x, s.y, s.width, s.height); };
    var C = PIXI.Container.prototype;
    if (!C.getBounds) C.getBounds = function() { return new PIXI.Rectangle(this.x, this.y, this.width || 0, this.height || 0); };
    if (!C.getLocalBounds) C.getLocalBounds = function() { return new PIXI.Rectangle(0, 0, this.width || 0, this.height || 0); };
    if (!C.toGlobal) C.toGlobal = function(p) { var x = p.x, y = p.y; for (var o = this; o; o = o.parent) { x += o.x || 0; y += o.y || 0; } return new PIXI.Point(x, y); };
    if (!C.toLocal) C.toLocal = function(p) { var g = this.toGlobal(new PIXI.Point(0, 0)); return new PIXI.Point(p.x - g.x, p.y - g.y); };
    if (!C.swapChildren) C.swapChildren = function(a, b) { var i = this.children.indexOf(a), j = this.children.indexOf(b); if (i >= 0 && j >= 0) { this.children[i] = b; this.children[j] = a; } };
    if (!C.getChildAt) C.getChildAt = function(i) { return this.children[i]; };
    if (!C.addChildren) C.addChildren = function() { for (var i = 0; i < arguments.length; i++) this.addChild(arguments[i]); };
    if (!C.setParent) C.setParent = function(p) { if (p) p.addChild(this); return p; };
    if (!C.calculateBounds) C.calculateBounds = function() {};
    if (!C.removeAllListeners) C.removeAllListeners = function() { this._events = null; return this; };
    if (!C.listeners) C.listeners = function(ev) { return (this._events && this._events[ev]) || []; };
    // Text drawn on its own canvas through the 2D context, shown like any sprite.
    if (!PIXI.Text) {
        PIXI.TextStyle = function TextStyle(s) { for (var k in s || {}) this[k] = s[k]; };
        PIXI.Text = function Text(text, style) {
            this.canvas = global.document.createElement('canvas');
            this.context = this.canvas.getContext('2d');
            PIXI.Sprite.call(this, new PIXI.Texture(new PIXI.BaseTexture(this.canvas)));
            this._text = ''; this.style = new PIXI.TextStyle(style); this.resolution = 1;
            this.text = text === undefined ? '' : text;
        };
        PIXI.Text.prototype = Object.create(PIXI.Sprite.prototype);
        PIXI.Text.prototype.constructor = PIXI.Text;
        PIXI.Text.prototype.updateText = function() {
            var s = this.style, size = parseInt(s.fontSize, 10) || 26, ctx = this.context;
            ctx.font = size + 'px GameFont';
            var w = Math.max(1, Math.ceil(ctx.measureText(this._text).width) + 8), h = Math.ceil(size * 1.4) + 4;
            this.canvas.width = w; this.canvas.height = h;
            ctx.clearRect(0, 0, w, h);
            ctx.font = size + 'px GameFont';
            ctx.fillStyle = typeof s.fill === 'number' ? PIXI.utils.hex2string(s.fill) : (s.fill || '#000000');
            ctx.fillText(this._text, 4, size + 2);
            this.texture.baseTexture.update();
            this.texture.frame = new PIXI.Rectangle(0, 0, w, h);
        };
        Object.defineProperty(PIXI.Text.prototype, 'text', {
            get: function() { return this._text; },
            set: function(v) { v = String(v === undefined || v === null ? '' : v); if (v !== this._text) { this._text = v; this.updateText(); } }
        });
    }
    if (!PIXI.loader) PIXI.loader = PIXI.Loader = { shared: null, add: function() { return this; }, load: function(cb) { if (cb) __queueAsync(function() { cb(PIXI.loader, {}); }); return this; }, resources: {}, reset: function() {} };
    if (!PIXI.ticker) PIXI.ticker = { shared: { add: function() {}, remove: function() {}, start: function() {}, stop: function() {}, deltaTime: 1 } };
    PIXI.Ticker = PIXI.ticker;
    if (!PIXI.particles) PIXI.particles = { ParticleContainer: PIXI.Container };
    PIXI.ParticleContainer = PIXI.ParticleContainer || PIXI.Container;
    if (!PIXI.extras.AnimatedSprite) PIXI.extras.AnimatedSprite = PIXI.Sprite;
    PIXI.AnimatedSprite = PIXI.AnimatedSprite || PIXI.extras.AnimatedSprite;
    PIXI.TilingSprite = PIXI.TilingSprite || PIXI.extras.TilingSprite;

    // ---- Node / NW.js ---------------------------------------------------------------------
    var proc = global.process;
    proc.env = proc.env || { HOME: '/game', NODE_ENV: 'production' };
    proc.argv = proc.argv || ['nw', '/game'];
    proc.execPath = proc.execPath || '/game/nw';
    proc.cwd = function() { return '/game'; };
    proc.exit = function() { __rpg2lx.requestExit(); };
    proc.nextTick = function(fn) { var a = Array.prototype.slice.call(arguments, 1); Promise.resolve().then(function() { fn.apply(null, a); }); };
    proc.hrtime = proc.hrtime || function(prev) {
        var ms = __rpg2lx.nowMs(), s = Math.floor(ms / 1000), ns = Math.floor((ms % 1000) * 1e6);
        return prev ? [s - prev[0], ns - prev[1]] : [s, ns];
    };
    proc.memoryUsage = function() { return { rss: 0, heapTotal: 0, heapUsed: 0, external: 0 }; };
    proc.once = proc.off = proc.removeListener = proc.emit = function() {};
    proc.stdout = proc.stderr = { write: function(s) { __rpg2lx.log(0, String(s)); return true; } };
    proc.mainModule.filename = '/game/index.html';
    proc.title = 'nw';
    proc.release = { name: 'node' };

    var win = global.nw.Window.get();
    win.title = global.document.title;
    win.isFullscreen = true;
    win.isKioskMode = false;
    ['enterFullscreen', 'leaveFullscreen', 'toggleFullscreen', 'enterKioskMode', 'leaveKioskMode', 'maximize', 'minimize',
     'restore', 'show', 'hide', 'setAlwaysOnTop', 'setResizable', 'setMinimumSize', 'setMaximumSize', 'setPosition',
     'resizeTo', 'moveTo', 'requestAttention', 'setProgressBar', 'removeAllListeners', 'removeListener', 'once', 'reload',
     'reloadIgnoringCache', 'closeDevTools', 'setShowInTaskbar', 'capturePage']
        .forEach(function(k) { if (!win[k]) win[k] = function() {}; });
    win.isDevToolsOpen = function() { return false; };
    win.window = global;
    global.nw.Window.open = function() {};
    global.nw.App.manifest = { name: 'game', main: 'index.html' };
    global.nw.App.dataPath = '/__rpg2lx_saves__';
    global.nw.App.closeAllWindows = global.nw.App.quit;
    global.nw.App.clearCache = function() {};
    global.nw.App.on = global.nw.App.registerGlobalHotKey = global.nw.App.unregisterGlobalHotKey = function() {};
    global.nw.Shell = { openExternal: function() {}, openItem: function() {}, showItemInFolder: function() {} };
    global.nw.Clipboard = { get: function() { return { get: function() { return ''; }, set: function() {}, clear: function() {} }; } };
    global.nw.Screen = { Init: function() {}, screens: [{ bounds: { x: 0, y: 0, width: 816, height: 624 }, scaleFactor: 1 }], on: function() {} };
    global.nw.Shortcut = function() {};
    global.nw.MenuItem = function() {};
    if (!global.nw.Menu.prototype.append) global.nw.Menu.prototype.append = function() {};

    function EventEmitter() { this._ev = {}; }
    EventEmitter.prototype.on = EventEmitter.prototype.addListener = function(n, f) { (this._ev[n] = this._ev[n] || []).push(f); return this; };
    EventEmitter.prototype.once = function(n, f) { var self = this; function w() { self.off(n, w); f.apply(this, arguments); } return this.on(n, w); };
    EventEmitter.prototype.off = EventEmitter.prototype.removeListener = function(n, f) { var l = this._ev[n] || []; var i = l.indexOf(f); if (i >= 0) l.splice(i, 1); return this; };
    EventEmitter.prototype.emit = function(n) { var a = Array.prototype.slice.call(arguments, 1), l = (this._ev[n] || []).slice(); for (var i = 0; i < l.length; i++) l[i].apply(this, a); return l.length > 0; };
    EventEmitter.prototype.removeAllListeners = function(n) { if (n) delete this._ev[n]; else this._ev = {}; return this; };

    // Node built-ins. fs works on the game folder (read) and the save folder (writes go there).
    var baseRequire = global.require;
    var fsMod = baseRequire('fs'), pathMod = baseRequire('path');
    function cb(fn) { return function() {
        var a = Array.prototype.slice.call(arguments), done = typeof a[a.length - 1] === 'function' ? a.pop() : null;
        var result, error = null;
        try { result = fn.apply(null, a); } catch (e) { error = e; }
        if (done) __queueAsync(function() { done(error, result); });
    }; }
    function enoent(p) { var e = new Error("ENOENT: no such file or directory, open '" + p + "'"); e.code = 'ENOENT'; e.errno = -2; e.path = p; return e; }
    var readFileSync = fsMod.readFileSync;
    // Like Node: no encoding gives a Buffer, and writes accept an encoding (base64 decodes to bytes).
    fsMod.readFileSync = function(p, opts) {
        if (!fsMod.existsSync(p)) throw enoent(p);
        var enc = typeof opts === 'string' ? opts : (opts && opts.encoding);
        var r = readFileSync(p, enc ? 'utf8' : null);
        if (r === null || r === undefined) throw enoent(p);
        if (!enc) return global.Buffer ? global.Buffer.from(r) : r;
        if (enc === 'base64' || enc === 'hex') {
            var bytes = readFileSync(p, null);
            return global.Buffer ? global.Buffer.from(bytes).toString(enc) : r;
        }
        return r;
    };
    var writeFileSync = fsMod.writeFileSync;
    fsMod.writeFileSync = function(p, data, opts) {
        var enc = typeof opts === 'string' ? opts : (opts && opts.encoding);
        if (typeof data === 'string' && enc === 'base64' && global.Buffer) data = global.Buffer.from(data, 'base64');
        writeFileSync(p, data);
    };
    function toBytes(d) {
        if (d instanceof ArrayBuffer) return new Uint8Array(d);
        if (d && ArrayBuffer.isView(d)) return new Uint8Array(d.buffer, d.byteOffset, d.byteLength);
        return new TextEncoder().encode(String(d));
    }
    fsMod.appendFileSync = function(p, data) {
        if (typeof data === 'string') {
            var old = fsMod.existsSync(p) ? readFileSync(p, 'utf8') || '' : '';
            return fsMod.writeFileSync(p, old + data);
        }
        var prev = fsMod.existsSync(p) ? readFileSync(p, null) : null, add = toBytes(data);
        var out = new Uint8Array((prev ? prev.length : 0) + add.length);
        if (prev) out.set(prev, 0);
        out.set(add, prev ? prev.length : 0);
        fsMod.writeFileSync(p, out);
    };
    fsMod.copyFileSync = function(a, b) {
        var bytes = readFileSync(a, null);
        if (bytes === null || bytes === undefined) throw enoent(a);
        fsMod.writeFileSync(b, bytes);
    };
    fsMod.renameSync = function(a, b) { fsMod.copyFileSync(a, b); fsMod.unlinkSync(a); };
    fsMod.rmSync = fsMod.rmdirSync = function(p) { fsMod.unlinkSync(p); };
    fsMod.accessSync = function(p) { if (!fsMod.existsSync(p)) throw enoent(p); };
    fsMod.lstatSync = fsMod.statSync;
    var statSync = fsMod.statSync;
    fsMod.statSync = function(p) {
        if (!fsMod.existsSync(p)) throw enoent(p);
        var s = statSync(p);
        s.size = s.isFile() ? (readFileSync(p) || { length: 0 }).length : 0;
        s.mtime = s.ctime = s.birthtime = new Date(0); s.mtimeMs = 0;
        s.isSymbolicLink = function() { return false; };
        return s;
    };
    fsMod.lstatSync = fsMod.statSync;
    var mkdirSync = fsMod.mkdirSync;
    fsMod.mkdirSync = function(p) { mkdirSync(p); return undefined; };
    fsMod.constants = { F_OK: 0, R_OK: 4, W_OK: 2, X_OK: 1 };
    fsMod.F_OK = 0; fsMod.R_OK = 4; fsMod.W_OK = 2; fsMod.X_OK = 1;
    ['readFile', 'writeFile', 'appendFile', 'mkdir', 'readdir', 'stat', 'lstat', 'unlink', 'rename', 'copyFile', 'access', 'rmdir', 'rm']
        .forEach(function(n) { fsMod[n] = cb(fsMod[n + 'Sync']); });
    fsMod.exists = function(p, done) { var r = fsMod.existsSync(p); if (done) __queueAsync(function() { done(r); }); };
    fsMod.promises = {};
    ['readFile', 'writeFile', 'appendFile', 'mkdir', 'readdir', 'stat', 'lstat', 'unlink', 'rename', 'copyFile', 'access', 'rmdir', 'rm']
        .forEach(function(n) { fsMod.promises[n] = function() { var a = arguments; return new Promise(function(res, rej) { try { res(fsMod[n + 'Sync'].apply(null, a)); } catch (e) { rej(e); } }); }; });
    fsMod.watch = fsMod.watchFile = function() { return { close: function() {} }; };
    fsMod.unwatchFile = function() {};
    fsMod.createWriteStream = function(p) { return { write: function(d) { fsMod.appendFileSync(p, d); return true; }, end: function() {}, on: function() { return this; } }; };

    pathMod.sep = '/';
    pathMod.delimiter = ':';
    pathMod.isAbsolute = function(p) { return /^([\/\\]|[a-zA-Z]:)/.test(String(p)); };
    pathMod.normalize = function(p) {
        var abs = String(p).charAt(0) === '/', out = [];
        String(p).replace(/\\/g, '/').split('/').forEach(function(s) {
            if (!s || s === '.') return;
            if (s === '..') { if (out.length && out[out.length - 1] !== '..') out.pop(); else if (!abs) out.push('..'); }
            else out.push(s);
        });
        return (abs ? '/' : '') + out.join('/') || (abs ? '/' : '.');
    };
    var join = pathMod.join;
    pathMod.join = function() { return pathMod.normalize(join.apply(null, arguments)); };
    pathMod.resolve = function() {
        var r = '';
        for (var i = arguments.length - 1; i >= 0 && !pathMod.isAbsolute(r); i--) if (arguments[i]) r = r ? arguments[i] + '/' + r : String(arguments[i]);
        if (!pathMod.isAbsolute(r)) r = '/game/' + r;
        return pathMod.normalize(r);
    };
    pathMod.relative = function(from, to) {
        var a = pathMod.resolve(from).split('/').filter(Boolean), b = pathMod.resolve(to).split('/').filter(Boolean), i = 0;
        while (i < a.length && i < b.length && a[i] === b[i]) i++;
        return a.slice(i).map(function() { return '..'; }).concat(b.slice(i)).join('/');
    };
    pathMod.parse = function(p) {
        var base = pathMod.basename(p), ext = pathMod.extname(p);
        return { root: pathMod.isAbsolute(p) ? '/' : '', dir: pathMod.dirname(p), base: base, ext: ext, name: base.slice(0, base.length - ext.length) };
    };
    pathMod.format = function(o) { return (o.dir ? o.dir + '/' : '') + (o.base || (o.name || '') + (o.ext || '')); };
    pathMod.posix = pathMod; pathMod.win32 = pathMod;

    var osMod = { platform: function() { return 'linux'; }, type: function() { return 'Linux'; }, arch: function() { return 'arm64'; },
        homedir: function() { return '/__rpg2lx_saves__'; }, tmpdir: function() { return '/__rpg2lx_saves__/tmp'; },
        hostname: function() { return 'rpg2lx'; }, EOL: '\n', cpus: function() { return [{}, {}, {}, {}]; },
        totalmem: function() { return 1024 * 1024 * 1024; }, freemem: function() { return 256 * 1024 * 1024; },
        release: function() { return '4.4'; }, userInfo: function() { return { username: 'player', homedir: '/__rpg2lx_saves__' }; },
        networkInterfaces: function() { return {}; } };
    var noProcess = { exec: function(c, o, done) { done = typeof o === 'function' ? o : done; if (done) __queueAsync(function() { done(new Error('child_process is not available'), '', ''); }); return new EventEmitter(); },
        execSync: function() { throw new Error('child_process is not available'); },
        spawn: function() { var e = new EventEmitter(); __queueAsync(function() { e.emit('error', new Error('child_process is not available')); }); return e; } };
    noProcess.execFile = noProcess.exec; noProcess.fork = noProcess.spawn; noProcess.spawnSync = noProcess.execSync; noProcess.execFileSync = noProcess.execSync;
    function Buffer(data) { return Buffer.from(data); }
    Buffer.from = function(data, encoding) {
        var bytes;
        if (typeof data === 'string') {
            if (encoding === 'base64') { var s = atob(data); bytes = new Uint8Array(s.length); for (var i = 0; i < s.length; i++) bytes[i] = s.charCodeAt(i); }
            else bytes = new TextEncoder().encode(data);
        } else if (data && ArrayBuffer.isView(data)) bytes = new Uint8Array(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
        else bytes = new Uint8Array(data instanceof ArrayBuffer ? data.slice(0) : (data || []));
        bytes.toString = function(enc) {
            if (enc === 'base64') { var t = ''; for (var i = 0; i < this.length; i++) t += String.fromCharCode(this[i]); return btoa(t); }
            if (enc === 'hex') { var h = ''; for (var j = 0; j < this.length; j++) h += (this[j] < 16 ? '0' : '') + this[j].toString(16); return h; }
            return new TextDecoder().decode(this);
        };
        return bytes;
    };
    Buffer.alloc = function(n) { return Buffer.from(new Uint8Array(n)); };
    Buffer.isBuffer = function(b) { return b instanceof Uint8Array; };
    Buffer.byteLength = function(s) { return new TextEncoder().encode(String(s)).length; };
    Buffer.concat = function(list) { var n = 0; list.forEach(function(b) { n += b.length; }); var out = new Uint8Array(n), o = 0; list.forEach(function(b) { out.set(b, o); o += b.length; }); return Buffer.from(out); };
    def('Buffer', Buffer);
    var modules = {
        fs: fsMod, path: pathMod, os: osMod, child_process: noProcess, 'nw.gui': global.nw,
        events: Object.assign(EventEmitter, { EventEmitter: EventEmitter }),
        util: { inherits: function(c, p) { c.prototype = Object.create(p.prototype); c.prototype.constructor = c; },
                format: function() { return Array.prototype.join.call(arguments, ' '); }, inspect: function(v) { try { return JSON.stringify(v); } catch (e) { return String(v); } },
                promisify: function(fn) { return function() { var a = Array.prototype.slice.call(arguments); return new Promise(function(res, rej) { a.push(function(e, r) { if (e) rej(e); else res(r); }); fn.apply(null, a); }); }; },
                TextEncoder: global.TextEncoder, TextDecoder: global.TextDecoder },
        buffer: { Buffer: Buffer },
        crypto: { randomBytes: function(n) { return Buffer.from(global.crypto.getRandomValues(new Uint8Array(n))); },
                  createHash: function() { var d = ''; return { update: function(s) { d += String(s); return this; }, digest: function() { var h = 0; for (var i = 0; i < d.length; i++) h = (h * 31 + d.charCodeAt(i)) | 0; return (h >>> 0).toString(16); } }; } },
        zlib: {}, http: {}, https: {}, net: {}, url: { parse: function(u) { return { href: u, pathname: u }; }, fileURLToPath: function(u) { return String(u).replace(/^file:\/\//, ''); }, pathToFileURL: function(p) { return { href: 'file://' + p }; } },
        assert: function(v, m) { if (!v) throw new Error(m || 'Assertion failed'); },
        timers: { setTimeout: setTimeout, clearTimeout: clearTimeout, setInterval: setInterval, clearInterval: clearInterval, setImmediate: global.setImmediate }
    };
    // Steam's greenworks needs a native .node library that cannot run here: games get a Steam that is not running.
    var greenworks = { initAPI: function() { return false; }, init: function() { return false; },
        isSteamRunning: function() { return false; }, getCurrentGameLanguage: function() { return 'english'; },
        getCurrentUILanguage: function() { return 'english'; }, getSteamId: function() { return { screenName: 'Player', steamId: '0', isValid: function() { return false; } }; },
        getNumberOfAchievements: function() { return 0; }, getAchievementNames: function() { return []; },
        isCloudEnabled: function() { return false; }, isCloudEnabledForUser: function() { return false; },
        isGameOverlayEnabled: function() { return false; }, on: function() {}, once: function() {}, removeAllListeners: function() {} };
    greenworks = typeof Proxy === 'function' ? new Proxy(greenworks, { get: function(t, k) {
        if (k in t) return t[k];
        // Anything else fails the way an offline Steam does: the error callback, if any, is called.
        return function() { for (var i = arguments.length - 1; i >= 0; i--) if (typeof arguments[i] === 'function') {
            var cb = arguments[i]; __queueAsync(function() { cb(new Error('Steam is not available')); }); break; } return false; };
    } }) : greenworks;
    global.require = function(name) {
        name = String(name).replace(/^node:/, '');
        if (Object.prototype.hasOwnProperty.call(modules, name)) return modules[name];
        if (/greenworks(\.js)?$/i.test(name)) return greenworks;
        var e = new Error("Cannot find module '" + name + "'");
        e.code = 'MODULE_NOT_FOUND';
        throw e;
    };
    global.require.resolve = function(n) { return n; };
    global.require.cache = {};
    global.require.main = { filename: '/game/index.html' };
    def('module', { exports: {} });
    def('__dirname', '/game');
    def('__filename', '/game/index.html');
    def('global', global);
})(globalThis);
)JS";

// Runs after rpg_core.js and rpg_managers.js: an exported game never runs as a playtest, so
// debug-only features (F8 dev tools, F9 debug menu, plugin editors gated on "test") stay off.
inline constexpr const char* kMvExportedModeJs = R"JS(
(function(global) {
    global.location.search = '';
    if (typeof Utils !== 'undefined') {
        Utils.isOptionValid = function(name) { return false; };
        // NW.js is what an exported desktop game runs on; plugins branch on it for fs/save paths.
        Utils.isNwjs = function() { return true; };
        Utils.isMobileDevice = function() { return false; };
        Utils.isMobileSafari = function() { return false; };
        Utils.isAndroidChrome = function() { return false; };
    }
    if (typeof Graphics !== 'undefined' && Graphics._switchFPSMeter) {
        // The FPS meter is a debug overlay; F2 toggles nothing in an exported build here.
        Graphics._switchFPSMeter = function() {};
    }
})(globalThis);
)JS";

} // namespace rpg2lx
