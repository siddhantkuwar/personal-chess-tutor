import fs from "node:fs";
import path from "node:path";

const [input, output] = process.argv.slice(2);
if (!input || !output) {
  console.error("usage: node scripts/xctrace-flamegraph.js profile.xml flamegraph.svg");
  process.exit(2);
}
const xml = fs.readFileSync(input, "utf8");
const decode = (value) => value.replaceAll("&lt;", "<").replaceAll("&gt;", ">")
  .replaceAll("&amp;", "&").replaceAll("&quot;", '"');
const frames = new Map();
for (const match of xml.matchAll(/<frame id="(\d+)" name="([^"]+)"/g)) frames.set(match[1], decode(match[2]));
const backtraces = new Map();
for (const match of xml.matchAll(/<backtrace id="(\d+)">([\s\S]*?)<\/backtrace>/g)) {
  const ids = [...match[2].matchAll(/<frame (?:id|ref)="(\d+)"/g)].map((item) => item[1]);
  backtraces.set(match[1], ids);
}
const tagged = new Map();
for (const match of xml.matchAll(/<tagged-backtrace id="(\d+)"[^>]*>([\s\S]*?)<\/tagged-backtrace>/g)) {
  const trace = match[2].match(/<backtrace (?:id|ref)="(\d+)"/);
  if (trace) tagged.set(match[1], trace[1]);
}

const stacks = new Map();
for (const match of xml.matchAll(/<row>([\s\S]*?)<\/row>/g)) {
  const row = match[1];
  let traceId;
  const reference = row.match(/<tagged-backtrace ref="(\d+)"/);
  if (reference) traceId = tagged.get(reference[1]);
  if (!traceId) {
    const inline = row.match(/<tagged-backtrace id="(\d+)"/);
    if (inline) traceId = tagged.get(inline[1]);
  }
  const trace = traceId ? backtraces.get(traceId) : undefined;
  if (!trace) continue;
  const names = trace.map((id) => frames.get(id)).filter(Boolean).reverse();
  if (!names.some((name) => name.includes("pct::") || name === "main")) continue;
  const key = names.join(";");
  stacks.set(key, (stacks.get(key) ?? 0) + 1);
}

const root = { name: "all samples", count: 0, children: new Map() };
for (const [stack, count] of stacks) {
  root.count += count;
  let node = root;
  for (const name of stack.split(";")) {
    if (!node.children.has(name)) node.children.set(name, { name, count: 0, children: new Map() });
    node = node.children.get(name);
    node.count += count;
  }
}
if (root.count === 0) throw new Error("no application samples found in xctrace export");

const width = 1200;
const frameHeight = 22;
const depth = (node) => 1 + Math.max(0, ...[...node.children.values()].map(depth));
const height = 70 + depth(root) * frameHeight;
const escape = (value) => value.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll('"', "&quot;");
const color = (name) => {
  let hash = 0;
  for (const character of name) hash = (hash * 31 + character.charCodeAt(0)) >>> 0;
  return `hsl(${20 + hash % 35} 70% ${58 + hash % 18}%)`;
};
const rectangles = [];
const render = (node, x, y, available) => {
  let cursor = x;
  const children = [...node.children.values()].sort((left, right) => right.count - left.count);
  for (const child of children) {
    const childWidth = available * child.count / node.count;
    if (childWidth >= 0.5) {
      const label = childWidth > 65 ? child.name : "";
      rectangles.push(`<g><title>${escape(child.name)} — ${child.count} samples</title><rect x="${cursor.toFixed(2)}" y="${y}" width="${Math.max(0, childWidth - 1).toFixed(2)}" height="20" fill="${color(child.name)}"/><text x="${(cursor + 4).toFixed(2)}" y="${y + 14}">${escape(label)}</text></g>`);
      render(child, cursor, y - frameHeight, childWidth);
    }
    cursor += childWidth;
  }
};
render(root, 10, height - 32, width - 20);
const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${width} ${height}" role="img" aria-labelledby="title desc"><title id="title">Personal Chess Tutor benchmark CPU flamegraph</title><desc id="desc">${root.count} sampled application stacks exported from Apple Instruments Time Profiler.</desc><style>text{font:11px ui-monospace,monospace;fill:#241f19;pointer-events:none}rect{stroke:#fff;stroke-width:.5}</style><rect width="100%" height="100%" fill="#f7f1e4"/><text x="10" y="20" style="font-size:15px;font-weight:bold">Benchmark CPU flamegraph · ${root.count} application samples</text>${rectangles.join("")}</svg>`;
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, svg);
