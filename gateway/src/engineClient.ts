import net from "node:net";
import { EventEmitter } from "node:events";
import { config } from "./config.ts";
import {
  encodeFrame,
  encodeLogin,
  encodeSubscribe,
  tryDecodeFrame,
  Type,
  type Frame,
} from "./protocol.ts";

export class EngineClient extends EventEmitter {
  private socket: net.Socket | null = null;
  private buf = Buffer.alloc(0);
  private seq = 1;
  private timer: NodeJS.Timeout | null = null;
  private participant = 0n;
  private symbols = new Set<number>();
  private stopped = false;
  connected = false;

  start(participant: bigint, symbols: number[]): void {
    this.stopped = false;
    this.participant = participant;
    this.symbols = new Set(symbols);
    this.connect();
  }

  stop(): void {
    this.stopped = true;
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    this.socket?.destroy();
    this.socket = null;
    this.connected = false;
  }

  send(type: number, payload: Buffer): void {
    if (!this.socket || !this.connected) {
      throw Object.assign(new Error("engine disconnected"), { status: 503 });
    }
    this.socket.write(encodeFrame(type, this.seq++, payload));
  }

  private connect(): void {
    if (this.socket) {
      this.socket.removeAllListeners();
      this.socket.destroy();
      this.socket = null;
    }
    const sock = net.connect({ host: config.engineHost, port: config.enginePort });
    this.socket = sock;
    this.buf = Buffer.alloc(0);
    sock.on("connect", () => {
      this.connected = true;
      this.send(Type.Login, encodeLogin(this.participant));
      for (const sym of this.symbols) {
        this.send(Type.Subscribe, encodeSubscribe(sym, true, true));
      }
      this.emit("status", "connected");
    });
    sock.on("data", (chunk: Buffer) => {
      this.buf = Buffer.concat([this.buf, chunk]);
      for (;;) {
        const r = tryDecodeFrame(this.buf);
        if (r.status === "need_more") {
          break;
        }
        if (r.status !== "ok" || !r.frame) {
          this.emit("error", new Error(`engine frame ${r.status}`));
          sock.destroy();
          return;
        }
        this.buf = this.buf.subarray(r.consumed);
        this.emit("frame", r.frame as Frame);
      }
    });
    sock.on("close", () => {
      this.connected = false;
      this.emit("status", "disconnected");
      this.scheduleReconnect();
    });
    sock.on("error", () => {
      /* close handler reconnects */
    });
  }

  private scheduleReconnect(): void {
    if (this.stopped || this.timer) {
      return;
    }
    this.timer = setTimeout(() => {
      this.timer = null;
      this.connect();
    }, 1000);
  }
}
