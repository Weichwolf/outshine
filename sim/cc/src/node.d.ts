// Minimal ambient declarations for the Node.js APIs this CC touches. @types/node is not a Debian package
// and we keep zero local deps, so we declare exactly what we use — Node built-ins only, nothing vendored.

declare const process: {
  argv: string[];
  env: Record<string, string | undefined>;
  stdout: { write(s: string): boolean };
  stderr: { write(s: string): boolean };
  exit(code?: number): never;
};
declare function setTimeout(cb: (...a: any[]) => void, ms: number): unknown;
declare function clearTimeout(h: unknown): void;
declare function fetch(url: string, init?: any): Promise<{
  ok: boolean; status: number; arrayBuffer(): Promise<ArrayBuffer>; text(): Promise<string>; json(): Promise<any>;
}>;

declare module 'node:net' {
  export interface Socket {
    setNoDelay(b: boolean): void;
    on(ev: 'data', cb: (d: Uint8Array) => void): this;
    on(ev: string, cb: (...a: any[]) => void): this;
    once(ev: string, cb: (...a: any[]) => void): this;
    write(data: Uint8Array): boolean;
    destroy(): void;
  }
  export function connect(opts: { host: string; port: number }): Socket;
}

// The 'ws' WebSocket client (Debian node-ws). Only the members the CC uses.
declare module 'ws' {
  export class WebSocket {
    constructor(url: string);
    binaryType: string;
    on(ev: 'open', cb: () => void): void;
    on(ev: 'message', cb: (data: Uint8Array) => void): void;
    on(ev: 'close', cb: () => void): void;
    on(ev: 'error', cb: (e: unknown) => void): void;
    send(data: Uint8Array): void;
    close(): void;
  }
}

interface ImportMeta { url: string; dirname: string; }
declare module 'node:path' {
  export function resolve(...parts: string[]): string;
  export function join(...parts: string[]): string;
}

declare module 'node:child_process' {
  export function execSync(cmd: string, opts?: { cwd?: string; maxBuffer?: number; encoding?: 'utf8' }): string;
  export function spawnSync(cmd: string, args?: string[], opts?: { encoding?: 'utf8'; maxBuffer?: number; input?: string; cwd?: string; env?: Record<string, string | undefined>; timeout?: number }): {
    status: number | null; stdout: string; stderr: string;
  };
}

declare module 'node:fs' {
  export function readFileSync(path: string, enc: 'utf8'): string;
  export function writeFileSync(path: string, data: string): void;
  export function existsSync(path: string): boolean;
}
