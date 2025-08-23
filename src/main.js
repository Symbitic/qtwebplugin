// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Remember to keep these in sync with QWebServer::Commands
const Commands = {
    INIT: 0x01,
    FRAMEBUFFER: 0x02,
};

function getWindowSize() {
    const actualWidth = window.innerWidth ||
        document.documentElement.clientWidth ||
        document.body.clientWidth ||
        document.body.offsetWidth;
    const actualHeight = window.innerHeight ||
        document.documentElement.clientHeight ||
        document.body.clientHeight ||
        document.body.offsetHeight;
    return {
        width: actualWidth,
        height: actualHeight
    };
}

function getPhysicalSize(width, height) {
    const div = document.createElement("div");
    div.style.width = "1mm";
    div.style.height = "1mm";
    const body = document.getElementsByTagName("body")[0];
    body.appendChild(div);
    const physicalWidth = document.defaultView.getComputedStyle(div, null).getPropertyValue("width");
    const physicalHeight = document.defaultView.getComputedStyle(div, null).getPropertyValue("height");
    body.removeChild(div);
    return {
        physicalWidth: width / parseFloat(physicalWidth),
        physicalHeight: height / parseFloat(physicalHeight)
    };
}

/**
 * Wrapper for handling integration between the canvas, input, websockets, and the Qt application.
 */
class WebIntegration {
    /**
     * Internal errors.
     * @readonly
     * @enum {number}
     * @private
     */
    static #ErrorCode = {
        SUCCESS: 0,
        MISSING_CANVAS: 1,
    }

    /**
     * Most recent error code.
     * @type {WebIntegration.#ErrorCode}
     * @private
     */
    #error = WebIntegration.#ErrorCode.SUCCESS;

    /**
     * The CSS selector used to locate the HTML5 CANVAS element.
     * @type {string}
     * @private
     */
    #selector;

    /**
     * Canvas element used for displaying the screen.
     * @type {HTMLElement}
     * @private
     */
    #canvas;

    /**
     * WebSocket used for communicating with the Qt application.
     * @type {WebSocket}
     * @private
     */
    #socket;

    /**
     * Create a new integration for handling communication with `QWebIntegration`.
     *
     * @param {string} selector - A valid CSS selector that should match an `HTMLElement`.
     */
    constructor(selector) {
        this.#selector = selector;
        this.#canvas = /** @type {HTMLElement} */ document.querySelector(selector);
        if (!this.#canvas) {
            this.#error = WebIntegration.#ErrorCode.MISSING_CANVAS;
            return;
        }
        this.#socket = new WebSocket("/ws");
        this.#socket.binaryType = "arraybuffer";
    }

    /**
     * Mandatory function to call after construction.
     */
    initialize() {
        if (this.#handleError()) {
            return;
        }

        // TODO: use setTimeout to display "Waiting for connection..."

        this.#socket.addEventListener("open", this.#sendInit.bind(this));
        this.#socket.addEventListener("message", this.#onSocketMessage.bind(this));
    }

    /**
     * Display an error message to the user.
     * @private
     */
    #handleError() {
        switch (this.#error) {
            case WebIntegration.#ErrorCode.MISSING_CANVAS:
                // TODO: create an element and display to the user.
                console.error(`Canvas missing - "${this.#selector}" not found`);
                return true;
            default:
                return false;
        }
    }

    /**
     * Sends the "INIT" command over WebSockets.
     * @private
     */
    #sendInit() {
        console.log("#sendInit");
        const { width, height } = getWindowSize();
        const { physicalWidth, physicalHeight } = getPhysicalSize(width, height);
        const buffer = new ArrayBuffer(21);
        const view = new DataView(buffer);
        view.setUint8(0, Commands.INIT);
        view.setUint16(1, width, true);
        view.setUint16(3, height, true);
        view.setFloat64(5, physicalWidth, true);
        view.setFloat64(13, physicalHeight, true);
        this.#socket.send(buffer);
    }

    /**
     * Handle a WebSocket message event.
     * @param {MessageEvent} event WebSocket message.
     * @private
     */
    #onSocketMessage(event) {
        if (!(event.data instanceof ArrayBuffer)) {
            console.dir("Invalid message", event);
            return;
        }
        const view = new DataView(event.data);
        switch (view.getUint8(0)) {
            case Commands.INIT:
                console.log("INIT");
                break;
            case Commands.FRAMEBUFFER: {
                console.log("FRAMEBUFFER");
                const _format = view.getUint8(1); // Currently unused.
                const rectCount = view.getUint16(2, true);
                let offset = 4;
                for (let i=0; i<rectCount; i++) {
                    // TODO: add bounds checks.
                    const x = view.getUint16(offset, true);
                    const y = view.getUint16(offset + 2, true);
                    const width = view.getUint16(offset + 4, true);
                    const height = view.getUint16(offset + 6, true);
                    const length = view.getUint32(offset + 8, true);
                    const byteOffset = offset + 12;
                    const data = new Uint8Array(view.buffer, byteOffset, length);
                    this.#drawTile(x, y, width, height, data);
                    offset = byteOffset + length;
                }
                break;
            }
            default:
                console.log("Unrecognized command:", view.getUint8(0));
                return;
        }
    }

    /**
     * Renders a piece of the screen.
     * @param {number} x Image x-coordinate.
     * @param {number} y Image y-coordinate.
     * @param {number} width Image width.
     * @param {number} height Image height.
     * @param {Uint8Array} data Image to render.
     */
    #drawTile(x, y, width, height, data) {
        // Convert premultiplied BGRA to straight RGBA
        const pixelCount = width * height;
        const dst = new Uint8ClampedArray(pixelCount * 4);

        let si = 0;
        let di = 0;
        for (let i = 0; i < pixelCount; i++) {
        const b = data[si++];
        const g = data[si++];
        const r = data[si++];
        const a = data[si++];

        if (a === 0) {
            dst[di++] = 0;
            dst[di++] = 0;
            dst[di++] = 0;
            dst[di++] = 0;
        } else if (a === 255) {
            // Already straight when fully opaque; just swizzle BGRA → RGBA
            dst[di++] = r;
            dst[di++] = g;
            dst[di++] = b;
            dst[di++] = 255;
        } else {
            // Un-premultiply each channel: straight = premult * (255 / a)
            const invA = 255 / a;
            dst[di++] = Math.min(255, Math.round(r * invA));
            dst[di++] = Math.min(255, Math.round(g * invA));
            dst[di++] = Math.min(255, Math.round(b * invA));
            dst[di++] = a;
        }
        }

        const imageData = new ImageData(dst, width, height);
        this.#canvas.getContext("2d").putImageData(imageData, x, y);
    }
}

addEventListener("DOMContentLoaded", () => {
    const instance = new WebIntegration("#qt");
    instance.initialize();
});
