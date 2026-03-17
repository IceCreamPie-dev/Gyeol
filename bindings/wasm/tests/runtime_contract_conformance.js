#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function readText(filePath) {
  return fs.readFileSync(filePath, "utf8");
}

function writeJson(filePath, value) {
  const dir = path.dirname(filePath);
  if (dir && dir !== ".") {
    fs.mkdirSync(dir, { recursive: true });
  }
  fs.writeFileSync(filePath, JSON.stringify(value, null, 2), "utf8");
}

function stableStringify(value) {
  if (Array.isArray(value)) {
    return `[${value.map((v) => stableStringify(v)).join(",")}]`;
  }
  if (value && typeof value === "object") {
    const keys = Object.keys(value).sort();
    return `{${keys.map((k) => `${JSON.stringify(k)}:${stableStringify(value[k])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

function jsValueToTypedVariant(value) {
  if (Array.isArray(value)) return { type: "list", value: value.slice() };
  if (typeof value === "boolean") return { type: "bool", value };
  if (typeof value === "number") {
    if (Number.isInteger(value)) return { type: "int", value };
    return { type: "float", value };
  }
  return { type: "string", value: value == null ? "" : String(value) };
}

function normalizeStepResult(result) {
  const type = String(result.type || "");
  if (type === "LINE") {
    const tagsObj = {};
    const tags = Array.isArray(result.tags) ? result.tags : [];
    for (const tag of tags) {
      const key = String(tag.key || "");
      const value = String(tag.value || "");
      tagsObj[key] = value;
    }
    return {
      type: "LINE",
      line: {
        character: result.character == null ? "" : String(result.character),
        text: String(result.text || ""),
        tags: tagsObj,
      },
    };
  }
  if (type === "CHOICES") {
    const choices = Array.isArray(result.choices) ? result.choices : [];
    return {
      type: "CHOICES",
      choices: choices.map((c) => ({
        index: Number(c.index || 0),
        text: String(c.text || ""),
      })),
    };
  }
  if (type === "COMMAND") {
    return {
      type: "COMMAND",
      command: {
        type: String(result.commandType || ""),
        params: (Array.isArray(result.params) ? result.params : []).map((p) => String(p || "")),
      },
    };
  }
  if (type === "WAIT") {
    return {
      type: "WAIT",
      wait: {
        tag: result.tag == null ? "" : String(result.tag),
      },
    };
  }
  if (type === "YIELD") {
    return { type: "YIELD" };
  }
  return { type: "END" };
}

function captureState(engine) {
  const names = Array.from(engine.getVariableNames()).map((n) => String(n)).sort();
  const variables = {};
  for (const name of names) {
    variables[name] = jsValueToTypedVariant(engine.getVariable(name));
  }
  return {
    finished: Boolean(engine.isFinished()),
    current_node: String(engine.getCurrentNodeName() || ""),
    variables,
  };
}

function jsonTypeName(value) {
  if (value === null) return "null";
  if (Array.isArray(value)) return "array";
  return typeof value;
}

function pushDiff(differences, pathExpr, kind, expected, actual) {
  const entry = { path: pathExpr, kind };
  if (expected !== undefined) entry.expected = expected;
  if (actual !== undefined) entry.actual = actual;
  differences.push(entry);
}

function collectDiffs(expected, actual, pathExpr, differences, maxDiffs, state) {
  if (differences.length >= maxDiffs) {
    state.truncated = true;
    return;
  }

  const expectedType = jsonTypeName(expected);
  const actualType = jsonTypeName(actual);
  if (expectedType !== actualType) {
    pushDiff(differences, pathExpr, "type_mismatch", expectedType, actualType);
    return;
  }

  if (Array.isArray(expected)) {
    if (expected.length !== actual.length) {
      pushDiff(differences, pathExpr, "array_size_mismatch", expected.length, actual.length);
      if (differences.length >= maxDiffs) {
        state.truncated = true;
        return;
      }
    }
    const minLength = Math.min(expected.length, actual.length);
    for (let i = 0; i < minLength; i += 1) {
      collectDiffs(expected[i], actual[i], `${pathExpr}[${i}]`, differences, maxDiffs, state);
      if (state.truncated) return;
    }
    return;
  }

  if (expected && typeof expected === "object") {
    const keys = new Set([...Object.keys(expected), ...Object.keys(actual)]);
    for (const key of Array.from(keys).sort()) {
      if (!(key in expected)) {
        pushDiff(differences, `${pathExpr}.${key}`, "unexpected_in_actual", undefined, actual[key]);
      } else if (!(key in actual)) {
        pushDiff(differences, `${pathExpr}.${key}`, "missing_in_actual", expected[key], undefined);
      } else {
        collectDiffs(expected[key], actual[key], `${pathExpr}.${key}`, differences, maxDiffs, state);
      }
      if (differences.length >= maxDiffs) {
        state.truncated = true;
        return;
      }
    }
    return;
  }

  if (expected !== actual) {
    pushDiff(differences, pathExpr, "value_mismatch", expected, actual);
  }
}

function makeDiffReport(expected, actual, expectedPath, actualPath) {
  const maxDiffs = 200;
  const differences = [];
  const state = { truncated: false };
  collectDiffs(expected, actual, "$", differences, maxDiffs, state);
  return {
    format: "gyeol-runtime-transcript-diff",
    version: 1,
    equal: differences.length === 0,
    expected_path: expectedPath,
    actual_path: actualPath,
    difference_count: differences.length,
    truncated: state.truncated,
    differences,
  };
}

async function main() {
  const args = process.argv.slice(2);
  if (args.length < 4) {
    console.error(
      "Usage: node runtime_contract_conformance.js <module.js> <story.gyeol> <actions.json> <golden.json> " +
        "[--expected-engine <engine>] [--expected-out <path>] [--actual-out <path>] [--diff-out <path>]"
    );
    process.exit(2);
  }

  const modulePathArg = args[0];
  const storyPathArg = args[1];
  const actionsPathArg = args[2];
  const goldenPathArg = args[3];
  const optionArgs = args.slice(4);

  let expectedEngine = "wasm";
  let expectedOutPath = "";
  let actualOutPath = "";
  let diffOutPath = "";

  for (let i = 0; i < optionArgs.length; i += 1) {
    const option = optionArgs[i];
    const next = optionArgs[i + 1];
    if (
      option === "--expected-engine" ||
      option === "--expected-out" ||
      option === "--actual-out" ||
      option === "--diff-out"
    ) {
      if (!next) {
        console.error(`Missing value for option: ${option}`);
        process.exit(2);
      }
      i += 1;
      if (option === "--expected-engine") expectedEngine = String(next);
      if (option === "--expected-out") expectedOutPath = String(next);
      if (option === "--actual-out") actualOutPath = String(next);
      if (option === "--diff-out") diffOutPath = String(next);
      continue;
    }

    console.error(`Unknown option: ${option}`);
    process.exit(2);
  }

  const modulePath = path.resolve(modulePathArg);
  const storyPath = path.resolve(storyPathArg);
  const actionsPath = path.resolve(actionsPathArg);
  const goldenPath = path.resolve(goldenPathArg);

  const createModule = require(modulePath);
  const wasmModule = await createModule();
  const engine = new wasmModule.GyeolEngine();

  const story = readText(storyPath);
  const compileResult = engine.compileAndLoad(story);
  if (!compileResult.success) {
    console.error("compileAndLoad failed:", JSON.stringify(compileResult, null, 2));
    process.exit(1);
  }

  const actionsDoc = readJson(actionsPath);
  if (
    actionsDoc.format !== "gyeol-runtime-actions" ||
    (actionsDoc.version !== 1 && actionsDoc.version !== 2) ||
    !Array.isArray(actionsDoc.actions)
  ) {
    console.error("Invalid actions document schema.");
    process.exit(1);
  }

  const transcript = {
    format: "gyeol-runtime-transcript",
    version: 2,
    engine: "wasm",
    steps: [],
    checkpoints: [],
  };

  for (const action of actionsDoc.actions) {
    if (!action || typeof action.op !== "string") {
      console.error("Action is missing string field 'op'.");
      process.exit(1);
    }

    const record = { action: action.op };
    if (action.op === "set_seed") {
      engine.setSeed(Number(action.seed || 0));
      record.seed = Number(action.seed || 0);
    } else if (action.op === "step") {
      record.result = normalizeStepResult(engine.step());
    } else if (action.op === "choose") {
      record.index = Number(action.index || 0);
      engine.choose(record.index);
    } else if (action.op === "resume") {
      record.ok = Boolean(engine.resume());
    } else if (action.op === "checkpoint") {
      record.label = String(action.label || "");
    } else {
      console.error(`Unsupported action op for WASM conformance: ${action.op}`);
      process.exit(1);
    }

    record.state = captureState(engine);
    if (action.op === "checkpoint") {
      transcript.checkpoints.push({ label: record.label, state: record.state });
    } else {
      transcript.steps.push(record);
    }
  }

  const golden = readJson(goldenPath);
  golden.engine = expectedEngine;

  if (expectedOutPath) {
    writeJson(path.resolve(expectedOutPath), golden);
  }
  if (actualOutPath) {
    writeJson(path.resolve(actualOutPath), transcript);
  }

  const diffReport = makeDiffReport(
    golden,
    transcript,
    goldenPath,
    actualOutPath ? path.resolve(actualOutPath) : "(in-memory)"
  );
  if (diffOutPath) {
    writeJson(path.resolve(diffOutPath), diffReport);
  }

  const actualStable = stableStringify(transcript);
  const expectedStable = stableStringify(golden);

  if (actualStable !== expectedStable) {
    console.error("WASM runtime contract mismatch.");
    if (diffOutPath) {
      console.error(`Diff report: ${path.resolve(diffOutPath)}`);
    }
    console.error(`Difference count: ${diffReport.difference_count}`);
    process.exit(1);
  }

  console.log("WASM runtime contract conformance passed.");
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
