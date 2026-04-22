"use strict";
// @ts-check
const vscode = require("vscode");
const path = require("path");
const fs = require("fs");

/** @param {string} s */
function splitPathList(s) {
  if (!s) return [];
  const sep = process.platform === "win32" ? ";" : ":";
  return s
    .split(sep)
    .map((x) => x.trim())
    .filter(Boolean);
}

/**
 * @param {string} scriptFsPath absolute path to current .flow file
 * @param {string} importRel path inside quotes
 * @param {string[]} extraRoots from settings + FLOW_PATH
 */
function resolveImportPath(scriptFsPath, importRel, extraRoots) {
  const rel = importRel.replace(/\\/g, "/");
  if (path.isAbsolute(rel)) {
    return fs.existsSync(rel) ? path.normalize(rel) : null;
  }
  const dir = path.dirname(scriptFsPath);
  const candidate = path.resolve(dir, importRel);
  if (fs.existsSync(candidate)) return candidate;
  for (const root of extraRoots) {
    const c = path.resolve(root, importRel);
    if (fs.existsSync(c)) return c;
  }
  return null;
}

/** @returns {string[]} */
function getSearchRoots() {
  const cfg = vscode.workspace.getConfiguration("flow");
  const fromSettings = cfg.get("importSearchPath");
  const arr = Array.isArray(fromSettings) ? fromSettings : [];
  const env = splitPathList(process.env.FLOW_PATH || "");
  return [...new Set([...arr.map(String), ...env])];
}

/**
 * @param {string} line
 * @returns {{ start: number, end: number, path: string } | null}
 */
function matchImportLine(line) {
  const m = /import\s+"([^"]+)"/.exec(line);
  if (!m || m.index === undefined) return null;
  const q0 = line.indexOf('"', m.index);
  const q1 = line.indexOf('"', q0 + 1);
  if (q0 < 0 || q1 <= q0) return null;
  return {
    start: q0 + 1,
    end: q1,
    path: m[1],
  };
}

/**
 * @param {vscode.TextDocument} document
 * @param {vscode.Position} position
 */
function findImportAtPosition(document, position) {
  const line = document.lineAt(position.line).text;
  const imp = matchImportLine(line);
  if (!imp) return null;
  const col = position.character;
  if (col < imp.start || col >= imp.end) return null;
  return { imp, line };
}

/** @param {string} id */
function reEscape(id) {
  return id.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

/**
 * @param {string} line
 * @param {string} name
 * @param {number} lineIndex
 * @returns {vscode.Range | null}
 */
function rangeForFuncOrLetDef(line, name, lineIndex) {
  const reFunc = new RegExp(
    `^(\\s*(?:export\\s+)?func\\s+)(${reEscape(name)})(\\s*\\()`
  );
  const reLet = new RegExp(
    `^(\\s*(?:export\\s+)?let\\s+)(${reEscape(name)})(\\b)`
  );
  let m = reFunc.exec(line);
  if (m) {
    const start = m[1].length;
    const end = start + m[2].length;
    return new vscode.Range(
      new vscode.Position(lineIndex, start),
      new vscode.Position(lineIndex, end)
    );
  }
  m = reLet.exec(line);
  if (m) {
    const start = m[1].length;
    const end = start + m[2].length;
    return new vscode.Range(
      new vscode.Position(lineIndex, start),
      new vscode.Position(lineIndex, end)
    );
  }
  return null;
}

/**
 * @param {vscode.TextDocument} document
 * @param {string} name
 * @returns {vscode.Location | null}
 */
function findDefInDocument(document, name) {
  for (let i = 0; i < document.lineCount; i++) {
    const line = document.lineAt(i).text;
    const r = rangeForFuncOrLetDef(line, name, i);
    if (r) return new vscode.Location(document.uri, r);
  }
  return null;
}

/**
 * @param {string} text
 * @param {string} filePath
 * @param {string} name
 * @returns {vscode.Location | null}
 */
function findDefInFileText(text, filePath, name) {
  const lines = text.split(/\r?\n/);
  const uri = vscode.Uri.file(filePath);
  for (let i = 0; i < lines.length; i++) {
    const r = rangeForFuncOrLetDef(lines[i], name, i);
    if (r) return new vscode.Location(uri, r);
  }
  return null;
}

/**
 * @param {string} filePath
 * @param {string[]} roots
 * @returns {string[]}
 */
function getDirectImportPaths(filePath, roots) {
  if (!fs.existsSync(filePath)) return [];
  let text;
  try {
    text = fs.readFileSync(filePath, "utf8");
  } catch {
    return [];
  }
  /** @type {string[]} */
  const out = [];
  for (const line of text.split(/\r?\n/)) {
    const m = /import\s+"([^"]+)"/.exec(line);
    if (!m) continue;
    const r = resolveImportPath(filePath, m[1], roots);
    if (r && fs.existsSync(r)) out.push(path.normalize(r));
  }
  return out;
}

/**
 * Search current buffer first, then BFS imported files for func / export let name.
 * @param {string} name
 * @param {vscode.TextDocument} document
 * @param {string} scriptPath
 * @param {string[]} roots
 * @returns {vscode.Location | null}
 */
function findDefinitionForName(name, document, scriptPath, roots) {
  const cur = findDefInDocument(document, name);
  if (cur) return cur;

  const startNorm = path.normalize(scriptPath);
  /** @type {string[]} */
  const queue = [];
  const seen = new Set([startNorm]);

  for (const p of getDirectImportPaths(scriptPath, roots)) {
    if (!seen.has(p)) {
      seen.add(p);
      queue.push(p);
    }
  }

  while (queue.length > 0) {
    const fp = queue.shift();
    if (!fp || !fs.existsSync(fp)) continue;
    let text;
    try {
      text = fs.readFileSync(fp, "utf8");
    } catch {
      continue;
    }
    const loc = findDefInFileText(text, fp, name);
    if (loc) return loc;
    for (const next of getDirectImportPaths(fp, roots)) {
      const n = path.normalize(next);
      if (!seen.has(n)) {
        seen.add(n);
        queue.push(n);
      }
    }
  }
  return null;
}

/**
 * @param {string} normalizedFp
 */
function getTextForPath(normalizedFp) {
  const doc = vscode.workspace.textDocuments.find(
    (d) => path.normalize(d.uri.fsPath) === normalizedFp
  );
  if (doc) return doc.getText();
  try {
    return fs.readFileSync(normalizedFp, "utf8");
  } catch {
    return "";
  }
}

/**
 * @param {string} filePath
 * @param {string} targetNorm normalized path of the defining module
 * @param {string[]} roots
 */
function fileImportsModule(filePath, targetNorm, roots) {
  const norm = path.normalize(filePath);
  if (!fs.existsSync(norm) && !vscode.workspace.textDocuments.some((d) => path.normalize(d.uri.fsPath) === norm))
    return false;
  const text = getTextForPath(norm);
  if (!text) return false;
  for (const line of text.split(/\r?\n/)) {
    const m = /import\s+"([^"]+)"/.exec(line);
    if (!m) continue;
    const resolved = resolveImportPath(norm, m[1], roots);
    if (resolved && path.normalize(resolved) === targetNorm) return true;
  }
  return false;
}

/**
 * @param {string} line
 * @param {number} col
 */
function isInLineComment(line, col) {
  const idx = line.indexOf("//");
  if (idx < 0) return false;
  return col >= idx;
}

/**
 * @param {string} line
 * @param {number} col
 */
function isInsideString(line, col) {
  let inStr = false;
  let esc = false;
  for (let i = 0; i < col && i < line.length; i++) {
    const c = line[i];
    if (esc) {
      esc = false;
      continue;
    }
    if (c === "\\" && inStr) {
      esc = true;
      continue;
    }
    if (c === '"') inStr = !inStr;
  }
  return inStr;
}

/**
 * @param {string} line
 * @param {number} lineIndex
 * @param {string} name
 * @param {string} filePath
 * @returns {vscode.Location[]}
 */
function findOccurrencesInLine(line, lineIndex, name, filePath) {
  const re = new RegExp(`\\b${reEscape(name)}\\b`, "g");
  /** @type {vscode.Location[]} */
  const out = [];
  let m;
  while ((m = re.exec(line)) !== null) {
    const startCol = m.index;
    if (isInLineComment(line, startCol)) continue;
    if (isInsideString(line, startCol)) continue;
    const endCol = startCol + m[0].length;
    const uri = vscode.Uri.file(filePath);
    out.push(
      new vscode.Location(
        uri,
        new vscode.Range(
          new vscode.Position(lineIndex, startCol),
          new vscode.Position(lineIndex, endCol)
        )
      )
    );
  }
  return out;
}

/**
 * @param {vscode.Range} a
 * @param {vscode.Range} b
 */
function rangeEquals(a, b) {
  return (
    a.start.line === b.start.line &&
    a.start.character === b.start.character &&
    a.end.line === b.end.line &&
    a.end.character === b.end.character
  );
}

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
  const definitionProvider = {
    /**
     * @param {vscode.TextDocument} document
     * @param {vscode.Position} position
     */
    provideDefinition(document, position) {
      if (document.languageId !== "flow") return null;
      const scriptPath = document.uri.fsPath;
      const roots = getSearchRoots();

      const importHit = findImportAtPosition(document, position);
      if (importHit) {
        const resolved = resolveImportPath(scriptPath, importHit.imp.path, roots);
        if (!resolved) return null;
        const uri = vscode.Uri.file(resolved);
        const pos = new vscode.Position(0, 0);
        return new vscode.Location(uri, new vscode.Range(pos, pos));
      }

      const wordRange = document.getWordRangeAtPosition(
        position,
        /[A-Za-z_][A-Za-z0-9_]*/
      );
      if (!wordRange) return null;
      const name = document.getText(wordRange);
      if (!name) return null;

      return findDefinitionForName(name, document, scriptPath, roots);
    },
  };

  const linkProvider = {
    /**
     * @param {vscode.TextDocument} document
     */
    provideDocumentLinks(document) {
      if (document.languageId !== "flow") return [];
      const scriptPath = document.uri.fsPath;
      const roots = getSearchRoots();
      /** @type {vscode.DocumentLink[]} */
      const links = [];
      for (let i = 0; i < document.lineCount; i++) {
        const line = document.lineAt(i).text;
        const imp = matchImportLine(line);
        if (!imp) continue;
        const resolved = resolveImportPath(scriptPath, imp.path, roots);
        if (!resolved) continue;
        const start = new vscode.Position(i, imp.start);
        const end = new vscode.Position(i, imp.end);
        const link = new vscode.DocumentLink(
          new vscode.Range(start, end),
          vscode.Uri.file(resolved)
        );
        link.tooltip = `Open ${resolved}`;
        links.push(link);
      }
      return links;
    },
  };

  const referenceProvider = {
    /**
     * @param {vscode.TextDocument} document
     * @param {vscode.Position} position
     * @param {vscode.ReferenceContext} refCtx
     */
    async provideReferences(document, position, refCtx) {
      if (document.languageId !== "flow") return null;
      const roots = getSearchRoots();
      const scriptPath = document.uri.fsPath;

      if (findImportAtPosition(document, position)) return [];

      const wordRange = document.getWordRangeAtPosition(
        position,
        /[A-Za-z_][A-Za-z0-9_]*/
      );
      if (!wordRange) return null;
      const name = document.getText(wordRange);
      if (!name) return null;

      const defLoc = findDefinitionForName(name, document, scriptPath, roots);
      if (!defLoc) return [];

      const defPath = path.normalize(defLoc.uri.fsPath);
      /** @type {Set<string>} */
      const toScan = new Set([defPath]);

      let uris = [];
      try {
        uris = await vscode.workspace.findFiles(
          "**/*.flow",
          "**/node_modules/**",
          10000
        );
      } catch {
        uris = [];
      }
      for (const u of uris) {
        const fp = path.normalize(u.fsPath);
        if (!fs.existsSync(fp)) continue;
        if (fp !== defPath && fileImportsModule(fp, defPath, roots)) {
          toScan.add(fp);
        }
      }

      /** @type {vscode.Location[]} */
      const all = [];
      for (const fp of toScan) {
        const text = getTextForPath(fp);
        const lines = text.split(/\r?\n/);
        for (let i = 0; i < lines.length; i++) {
          const locs = findOccurrencesInLine(lines[i], i, name, fp);
          for (const loc of locs) {
            const isDecl =
              path.normalize(loc.uri.fsPath) === path.normalize(defLoc.uri.fsPath) &&
              rangeEquals(loc.range, defLoc.range);
            if (!refCtx.includeDeclaration && isDecl) continue;
            all.push(loc);
          }
        }
      }

      if (refCtx.includeDeclaration) {
        const hasDecl = all.some(
          (l) =>
            path.normalize(l.uri.fsPath) === path.normalize(defLoc.uri.fsPath) &&
            rangeEquals(l.range, defLoc.range)
        );
        if (!hasDecl) all.push(defLoc);
      }

      return all;
    },
  };

  context.subscriptions.push(
    vscode.languages.registerDefinitionProvider(
      { language: "flow", scheme: "file" },
      definitionProvider
    ),
    vscode.languages.registerReferenceProvider(
      { language: "flow", scheme: "file" },
      referenceProvider
    ),
    vscode.languages.registerDocumentLinkProvider(
      { language: "flow", scheme: "file" },
      linkProvider
    )
  );
}

function deactivate() {}

module.exports = { activate, deactivate };
