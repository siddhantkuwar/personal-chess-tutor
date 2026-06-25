#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const rootsToScan = ["README.md", "release", "packaging/macos", "docs"]
  .map((entry) => path.join(root, entry))
  .filter((entry) => fs.existsSync(entry));

const allowedExtensions = new Set([".html", ".md"]);
const failures = [];

function walk(entry) {
  const stat = fs.statSync(entry);
  if (stat.isDirectory()) {
    return fs.readdirSync(entry).flatMap((child) => walk(path.join(entry, child)));
  }
  return allowedExtensions.has(path.extname(entry)) ? [entry] : [];
}

function stripFragment(target) {
  const hash = target.indexOf("#");
  if (hash === -1) {
    return { file: target, fragment: "" };
  }
  return { file: target.slice(0, hash), fragment: target.slice(hash + 1) };
}

function isExternal(target) {
  return /^(https?:|mailto:|tel:|ws:|wss:|data:)/i.test(target);
}

function decodeUrlPath(target, source) {
  try {
    return decodeURIComponent(target);
  } catch (error) {
    failures.push(`${relative(source)} has an invalid URL escape in ${target}`);
    return target;
  }
}

function relative(file) {
  return path.relative(root, file) || ".";
}

function idsFor(file) {
  const text = fs.readFileSync(file, "utf8");
  const ids = new Set();
  for (const match of text.matchAll(/\bid=["']([^"']+)["']/g)) {
    ids.add(match[1]);
  }
  for (const match of text.matchAll(/\bname=["']([^"']+)["']/g)) {
    ids.add(match[1]);
  }
  return ids;
}

function markdownHeadingId(line) {
  return line
    .replace(/^#+\s*/, "")
    .trim()
    .toLowerCase()
    .replace(/`([^`]+)`/g, "$1")
    .replace(/[^a-z0-9 _-]/g, "")
    .replace(/\s+/g, "-");
}

function markdownIdsFor(file) {
  const ids = new Set();
  for (const line of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    if (/^#{1,6}\s+/.test(line)) {
      ids.add(markdownHeadingId(line));
    }
  }
  return ids;
}

function validateReference(source, rawTarget, kind) {
  if (!rawTarget || rawTarget.startsWith("#") || isExternal(rawTarget)) {
    return;
  }
  if (rawTarget.startsWith("javascript:")) {
    failures.push(`${relative(source)} uses javascript URL ${rawTarget}`);
    return;
  }

  const { file, fragment } = stripFragment(rawTarget);
  const decoded = decodeUrlPath(file, source);
  const resolved = path.resolve(path.dirname(source), decoded || ".");

  if (!resolved.startsWith(root + path.sep) && resolved !== root) {
    failures.push(`${relative(source)} ${kind} escapes repository root: ${rawTarget}`);
    return;
  }

  if (!fs.existsSync(resolved)) {
    failures.push(`${relative(source)} ${kind} target is missing: ${rawTarget}`);
    return;
  }

  if (fragment && fs.statSync(resolved).isFile()) {
    const ext = path.extname(resolved);
    const ids = ext === ".md" ? markdownIdsFor(resolved) : idsFor(resolved);
    if ((ext === ".html" || ext === ".md") && !ids.has(fragment)) {
      failures.push(`${relative(source)} ${kind} anchor is missing: ${rawTarget}`);
    }
  }
}

function linksFromHtml(text) {
  const refs = [];
  for (const match of text.matchAll(/\b(?:href|src)=["']([^"']+)["']/g)) {
    refs.push(match[1]);
  }
  return refs;
}

function linksFromMarkdown(text) {
  const refs = [];
  for (const match of text.matchAll(/!?\[[^\]]*]\(([^)\s]+)(?:\s+["'][^"']*["'])?\)/g)) {
    refs.push(match[1]);
  }
  return refs;
}

const files = rootsToScan.flatMap((entry) => walk(entry)).sort();
for (const file of files) {
  const text = fs.readFileSync(file, "utf8");
  const refs = path.extname(file) === ".md" ? linksFromMarkdown(text) : linksFromHtml(text);
  for (const ref of refs) {
    validateReference(file, ref, "link");
  }
}

if (failures.length > 0) {
  console.error("Release/docs link check failed:");
  for (const failure of failures) {
    console.error(`- ${failure}`);
  }
  process.exit(1);
}

console.log(`Checked ${files.length} HTML/Markdown files for local links.`);
