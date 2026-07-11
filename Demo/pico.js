// pico.js - Picotility web runtime
// Handles WASM loading, rendering, audio, input, and the main loop.
// createPicotilityModule is a global set by build/picotility.js (Emscripten MODULARIZE)

// PICO-8 palette (RGB)
const PALETTE = [
    [0,0,0],       [29,43,83],    [126,37,83],   [0,135,81],
    [171,82,54],   [95,87,79],    [194,195,199], [255,241,232],
    [255,0,77],    [255,163,0],   [255,236,39],  [0,228,54],
    [41,173,255],  [131,118,156], [255,119,168], [255,204,170],
];

// Flatten to Uint8Array for fast lookups: paletteRGB[color*3..color*3+2]
const PAL_FLAT = new Uint8Array(16 * 3);
for (let i = 0; i < 16; i++) {
    PAL_FLAT[i * 3]     = PALETTE[i][0];
    PAL_FLAT[i * 3 + 1] = PALETTE[i][1];
    PAL_FLAT[i * 3 + 2] = PALETTE[i][2];
}

// Button constants (must match pico_input.h)
const BTN = { LEFT: 1, RIGHT: 2, UP: 4, DOWN: 8, O: 16, X: 32, MENU: 64 };

// Key mapping
const KEY_MAP = {
    ArrowLeft:  BTN.LEFT,
    ArrowRight: BTN.RIGHT,
    ArrowUp:    BTN.UP,
    ArrowDown:  BTN.DOWN,
    KeyZ:       BTN.O,
    Space:      BTN.O,
    KeyX:       BTN.X,
    ShiftLeft:  BTN.X,
    ShiftRight: BTN.X,
    Enter:      BTN.MENU,
};

// State
let mod = null;         // Emscripten module instance
let wasm = {};          // Wrapped WASM functions
let mem = null;         // Module HEAPU8
let audioCtx = null;
let audioRunning = false;
let rafId = 0;
let lastFrameTime = 0;
let canvas, ctx, imageData;

// DOM refs
const $canvas      = () => document.getElementById("screen");
const $error       = () => document.getElementById("error-overlay");
const $errorText   = () => document.getElementById("error-text");
const $fpsDisplay  = () => document.getElementById("fps-display");
const $frameDisplay = () => document.getElementById("frame-display");
const $examples    = () => document.getElementById("examples");
const $cartList    = () => document.getElementById("cart-list");
const $dropZone    = () => document.getElementById("drop-zone");
const $fileInput   = () => document.getElementById("file-input");

// Init
async function init() {
    mod = await createPicotilityModule();

    // Wrap exported C functions
    const wrap = (name, ret, args) => mod.cwrap(name, ret, args);
    wasm.init            = wrap("wasm_init",           "number", []);
    wasm.shutdown        = wrap("wasm_shutdown",       "",       []);
    wasm.run             = wrap("wasm_run",            "",       []);
    wasm.step            = wrap("wasm_step",           "",       []);
    wasm.stop            = wrap("wasm_stop",           "",       []);
    wasm.setButton       = wrap("wasm_set_button",     "",       ["number","number"]);
    wasm.getFramebuffer  = wrap("wasm_get_framebuffer","number", []);
    wasm.getDirtyTop     = wrap("wasm_get_dirty_top",  "number", []);
    wasm.getDirtyBottom  = wrap("wasm_get_dirty_bottom","number",[]);
    wasm.needsFlip       = wrap("wasm_needs_flip",     "number", []);
    wasm.flip            = wrap("wasm_flip",           "",       []);
    wasm.audioFill       = wrap("wasm_audio_fill",     "number", ["number","number"]);
    wasm.getError        = wrap("wasm_get_error",      "number", []);
    wasm.getFps          = wrap("wasm_get_fps",        "number", []);
    wasm.getFrameCount   = wrap("wasm_get_frame_count","number", []);
    wasm.getState        = wrap("wasm_get_state",      "number", []);
    wasm.malloc          = wrap("malloc",              "number", ["number"]);
    wasm.free            = wrap("free",                "",       ["number"]);

    const rc = wasm.init();
    if (rc !== 0) {
        showError("Failed to initialize emulator");
        return;
    }

    // HEAPU8 is exposed via EM_JS during wasm_init
    mem = globalThis.__picoHeapU8;
    if (!mem) {
        showError("Failed to access WASM memory");
        return;
    }

    setupCanvas();
    setupInput();
    setupFileInput();
    setupDragDrop();
    await discoverCarts();
}

// Canvas
function setupCanvas() {
    canvas = $canvas();
    ctx = canvas.getContext("2d");
    imageData = ctx.createImageData(128, 128);
    // Fill with black initially
    imageData.data.fill(0);
    imageData.data.fill(255, 3);  // alpha = 255
    ctx.putImageData(imageData, 0, 0);
}

function renderFrame() {
    const fbPtr = wasm.getFramebuffer();
    const top   = wasm.getDirtyTop();
    const bottom = wasm.getDirtyBottom();

    const rgba = imageData.data;

    // Convert 4bpp packed framebuffer to RGBA
    // Screen is 128x128, 2 pixels per byte, row stride = 64 bytes
    for (let row = top; row <= bottom; row++) {
        const rowBase = fbPtr + row * 64;
        const pixRow  = row * 128 * 4;
        for (let col = 0; col < 64; col++) {
            const b = mem[rowBase + col];
            const lo = b & 0x0f;
            const hi = (b >> 4) & 0x0f;

            const i0 = pixRow + col * 8;
            const i1 = i0 + 4;

            rgba[i0]     = PAL_FLAT[lo * 3];
            rgba[i0 + 1] = PAL_FLAT[lo * 3 + 1];
            rgba[i0 + 2] = PAL_FLAT[lo * 3 + 2];

            rgba[i1]     = PAL_FLAT[hi * 3];
            rgba[i1 + 1] = PAL_FLAT[hi * 3 + 1];
            rgba[i1 + 2] = PAL_FLAT[hi * 3 + 2];
        }
    }

    ctx.putImageData(imageData, 0, 0);
    wasm.flip();
}

// Main loop
function mainLoop(timestamp) {
    rafId = requestAnimationFrame(mainLoop);

    const fps = wasm.getFps();
    if (fps === 0) return;  // no cart loaded
    const interval = 1000 / fps;

    if (timestamp - lastFrameTime < interval) return;
    lastFrameTime = timestamp;

    if (wasm.getState() !== 1) return;  // 1 = RUNNING

    wasm.step();

    if (wasm.needsFlip()) {
        renderFrame();
    }

    const $fps = $fpsDisplay();
    const $frame = $frameDisplay();
    if ($fps) $fps.textContent = fps + " fps";
    if ($frame) $frame.textContent = wasm.getFrameCount();
}

function stopLoop() {
    if (rafId) cancelAnimationFrame(rafId);
    rafId = 0;
    lastFrameTime = 0;
}

// Cart loading
async function loadCartFromBuffer(name, data) {
    stopLoop();
    hideError();
    wasm.stop();

    // Use ccall with 'array' type to pass data (handles heap allocation internally)
    const rc = mod.ccall("wasm_load_cart", "number", ["array", "number"], [data, data.length]);

    if (rc !== 0) {
        const errPtr = wasm.getError();
        const msg = errPtr ? mod.UTF8ToString(errPtr) : "Unknown error";
        showError(msg);
        return;
    }

    console.log("Loaded:", name, "-", wasm.getFps() + " fps");
    wasm.run();
    lastFrameTime = 0;
    rafId = requestAnimationFrame(mainLoop);
}

async function loadCartFromURL(url) {
    const name = url.split("/").pop();
    try {
        const resp = await fetch(url);
        if (!resp.ok) throw new Error("HTTP " + resp.status);
        const buf = new Uint8Array(await resp.arrayBuffer());
        await loadCartFromBuffer(name, buf);
    } catch (e) {
        showError("Failed to load " + name + ": " + e.message);
    }
}

// File input
function setupFileInput() {
    $fileInput().addEventListener("change", async (e) => {
        const file = e.target.files[0];
        if (!file) return;
        const buf = new Uint8Array(await file.arrayBuffer());
        await loadCartFromBuffer(file.name, buf);
        e.target.value = "";
    });
}

// Drag & drop
function setupDragDrop() {
    const zone = $dropZone();
    const prevent = (e) => { e.preventDefault(); e.stopPropagation(); };

    zone.addEventListener("dragenter", (e) => { prevent(e); zone.classList.add("drag-over"); });
    zone.addEventListener("dragover",  (e) => { prevent(e); zone.classList.add("drag-over"); });
    zone.addEventListener("dragleave", () => { zone.classList.remove("drag-over"); });
    zone.addEventListener("drop", async (e) => {
        prevent(e);
        zone.classList.remove("drag-over");
        const file = e.dataTransfer.files[0];
        if (!file) return;
        const buf = new Uint8Array(await file.arrayBuffer());
        await loadCartFromBuffer(file.name, buf);
    });
}

// Keyboard input
const heldButtons = new Set();

function setupInput() {
    document.addEventListener("keydown", (e) => {
        const btn = KEY_MAP[e.code];
        if (btn !== undefined && !heldButtons.has(btn)) {
            heldButtons.add(btn);
            wasm.setButton(btn, 1);
            e.preventDefault();
        }
        // Resume audio on first interaction
        resumeAudio();
    });

    document.addEventListener("keyup", (e) => {
        const btn = KEY_MAP[e.code];
        if (btn !== undefined) {
            heldButtons.delete(btn);
            wasm.setButton(btn, 0);
            e.preventDefault();
        }
    });

    // Touch controls
    document.querySelectorAll(".dpad-btn, .action-btn").forEach((el) => {
        const btn = parseInt(el.dataset.btn, 10);

        el.addEventListener("touchstart", (e) => {
            e.preventDefault();
            heldButtons.add(btn);
            wasm.setButton(btn, 1);
            resumeAudio();
        }, { passive: false });

        el.addEventListener("touchend", (e) => {
            e.preventDefault();
            heldButtons.delete(btn);
            wasm.setButton(btn, 0);
        }, { passive: false });

        el.addEventListener("touchcancel", () => {
            heldButtons.delete(btn);
            wasm.setButton(btn, 0);
        });
    });
}

// Audio
const AUDIO_BUFFER_SIZE = 1024;
const AUDIO_SAMPLE_RATE = 22050;

function resumeAudio() {
    if (audioCtx && audioCtx.state === "suspended") {
        audioCtx.resume();
    }
    if (!audioRunning) {
        startAudio();
    }
}

function startAudio() {
    try {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)({
            sampleRate: AUDIO_SAMPLE_RATE,
        });
    } catch (e) {
        console.warn("Web Audio not available:", e);
        return;
    }

    audioRunning = true;

    // Allocate a buffer in WASM heap for audio reads
    const audioBufPtr = wasm.malloc(AUDIO_BUFFER_SIZE * 2);  // int16_t = 2 bytes
    if (!audioBufPtr) {
        console.warn("Failed to allocate audio buffer");
        return;
    }

    const scriptNode = audioCtx.createScriptProcessor(AUDIO_BUFFER_SIZE, 0, 1);
    scriptNode.onaudioprocess = (e) => {
        const output = e.outputBuffer.getChannelData(0);
        const n = wasm.audioFill(audioBufPtr, AUDIO_BUFFER_SIZE);

        // Convert int16 PCM to float32
        for (let i = 0; i < n; i++) {
            const sample = mem[audioBufPtr + i * 2] | (mem[audioBufPtr + i * 2 + 1] << 8);
            output[i] = (sample < 0 ? sample + 32768 : sample - 32768) / 32768;
        }
        // Zero remaining if underfilled
        for (let i = n; i < AUDIO_BUFFER_SIZE; i++) {
            output[i] = 0;
        }
    };

    scriptNode.connect(audioCtx.destination);
}

// Bundled cart discovery
async function discoverCarts() {
    // Try to fetch a directory listing or manifest.
    // Falls back to a hardcoded list if listing isn't available.
    let cartFiles = [];

    try {
        // Try fetching a manifest file if you maintain one
        const manifest = await fetch("carts/manifest.json");
        if (manifest.ok) {
            cartFiles = await manifest.json();
        }
    } catch (_) {}

    if (cartFiles.length === 0) {
        // Fallback: try known cart names
        const candidates = [
            "racer.p8",
            "splittest.p8",
            "tvstatic.p8.png",
            "psetall.p8",
            "fillptest.p8",
        ];
        for (const name of candidates) {
            try {
                const resp = await fetch("carts/" + name, { method: "HEAD" });
                if (resp.ok) cartFiles.push(name);
            } catch (_) {}
        }
    }

    if (cartFiles.length === 0) return;

    const ul = $cartList();
    ul.innerHTML = "";
    for (const name of cartFiles) {
        const li = document.createElement("li");
        li.textContent = name.replace(/\.(p8|png)$/, "");
        li.addEventListener("click", () => loadCartFromURL("carts/" + name));
        ul.appendChild(li);
    }
    $examples().classList.remove("hidden");
}

// Error display
function showError(msg) {
    const overlay = $error();
    const text = $errorText();
    if (overlay) overlay.classList.remove("hidden");
    if (text) text.textContent = msg;
    console.error("Picotility:", msg);
}

function hideError() {
    const overlay = $error();
    if (overlay) overlay.classList.add("hidden");
}

// Boot
init().catch((e) => showError(String(e)));
