globalThis.punpunIdeExtension = {
  name: "PunPun Builtins",
  languages: {
    punpun: {
      keywords: ["fn","craft","let","mut","const","return","give","if","else","while","for","match","struct","enum","impl","trait","use","import","extern","async","await","launch","done","say","true","false","none","as","gives"],
      snippets: {
        "fn": "fn name(arg: int) -> int {\n    return arg;\n}",
        "launch": "launch {\n    say(\"Hello from PunPun\");\n}",
        "match": "match value {\n    _ => { }\n}"
      }
    },
    cpp: { keywords: ["constexpr","concept","requires","co_await","co_return","namespace","template","typename"] },
    markdown: { keywords: [] }
  }
};
