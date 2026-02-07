#include <gtest/gtest.h>
#include "gyeol_analyzer.h"
#include "lsp_server.h"

using namespace Gyeol;

// ==========================================================================
// Analyzer 심볼 스캔 테스트
// ==========================================================================

TEST(AnalyzerTest, ScanLabels) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "label start:\n"
        "    hero \"hello\"\n"
        "label other:\n"
        "    \"bye\"\n"
    );

    auto& labels = analyzer.getLabels();
    EXPECT_EQ(labels.size(), 2u);
    EXPECT_EQ(labels[0].name, "start");
    EXPECT_EQ(labels[0].line, 0);
    EXPECT_EQ(labels[1].name, "other");
    EXPECT_EQ(labels[1].line, 2);
}

TEST(AnalyzerTest, ScanLabelParams) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "label add(a, b):\n"
        "    return a + b\n"
    );

    auto& labels = analyzer.getLabels();
    ASSERT_EQ(labels.size(), 1u);
    EXPECT_EQ(labels[0].name, "add");
    ASSERT_EQ(labels[0].params.size(), 2u);
    EXPECT_EQ(labels[0].params[0], "a");
    EXPECT_EQ(labels[0].params[1], "b");
}

TEST(AnalyzerTest, ScanLabelNoParams) {
    Analyzer analyzer;
    analyzer.scanSymbols("label mynode:\n    hero \"hi\"\n");

    auto& labels = analyzer.getLabels();
    ASSERT_EQ(labels.size(), 1u);
    EXPECT_EQ(labels[0].name, "mynode");
    EXPECT_TRUE(labels[0].params.empty());
}

TEST(AnalyzerTest, ScanVariables) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "$ global_var = 10\n"
        "label start:\n"
        "    $ local_var = true\n"
        "    $ another = \"hello\"\n"
    );

    auto& vars = analyzer.getVariables();
    ASSERT_EQ(vars.size(), 3u);

    EXPECT_EQ(vars[0].name, "global_var");
    EXPECT_TRUE(vars[0].isGlobal);
    EXPECT_EQ(vars[0].line, 0);

    EXPECT_EQ(vars[1].name, "local_var");
    EXPECT_FALSE(vars[1].isGlobal);

    EXPECT_EQ(vars[2].name, "another");
    EXPECT_FALSE(vars[2].isGlobal);
}

TEST(AnalyzerTest, ScanVariableDedup) {
    // 같은 변수 이름의 중복 선언 → 첫 선언만 유지
    Analyzer analyzer;
    analyzer.scanSymbols(
        "label start:\n"
        "    $ x = 1\n"
        "    $ x = 2\n"
        "    $ x = 3\n"
    );

    auto& vars = analyzer.getVariables();
    EXPECT_EQ(vars.size(), 1u);
    EXPECT_EQ(vars[0].name, "x");
    EXPECT_EQ(vars[0].line, 1);
}

TEST(AnalyzerTest, ScanJumpRefs) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "label start:\n"
        "    jump other\n"
        "    call helper\n"
        "label other:\n"
        "    \"bye\"\n"
        "label helper:\n"
        "    \"help\"\n"
    );

    auto& refs = analyzer.getJumpRefs();
    ASSERT_EQ(refs.size(), 2u);
    EXPECT_EQ(refs[0].targetName, "other");
    EXPECT_EQ(refs[0].line, 1);
    EXPECT_EQ(refs[1].targetName, "helper");
    EXPECT_EQ(refs[1].line, 2);
}

TEST(AnalyzerTest, ScanChoiceArrowRefs) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "label start:\n"
        "    menu:\n"
        "        \"Go left\" -> left\n"
        "        \"Go right\" -> right\n"
        "label left:\n"
        "    \"L\"\n"
        "label right:\n"
        "    \"R\"\n"
    );

    auto& refs = analyzer.getJumpRefs();
    ASSERT_EQ(refs.size(), 2u);
    EXPECT_EQ(refs[0].targetName, "left");
    EXPECT_EQ(refs[1].targetName, "right");
}

TEST(AnalyzerTest, ScanConditionArrowRef) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "label start:\n"
        "    if x > 5 -> yes\n"
        "label yes:\n"
        "    \"Y\"\n"
    );

    auto& refs = analyzer.getJumpRefs();
    ASSERT_GE(refs.size(), 1u);
    EXPECT_EQ(refs[0].targetName, "yes");
}

TEST(AnalyzerTest, ScanCallWithParens) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "label start:\n"
        "    call func(1, 2)\n"
        "label func(a, b):\n"
        "    return a + b\n"
    );

    auto& refs = analyzer.getJumpRefs();
    ASSERT_GE(refs.size(), 1u);
    EXPECT_EQ(refs[0].targetName, "func");
}

TEST(AnalyzerTest, ScanIgnoresComments) {
    Analyzer analyzer;
    analyzer.scanSymbols(
        "# this is a comment\n"
        "label start:\n"
        "    # another comment\n"
        "    hero \"hi\"\n"
    );

    auto& labels = analyzer.getLabels();
    EXPECT_EQ(labels.size(), 1u);
    EXPECT_EQ(labels[0].name, "start");
}

TEST(AnalyzerTest, ScanEmptyContent) {
    Analyzer analyzer;
    analyzer.scanSymbols("");

    EXPECT_TRUE(analyzer.getLabels().empty());
    EXPECT_TRUE(analyzer.getVariables().empty());
    EXPECT_TRUE(analyzer.getJumpRefs().empty());
}

TEST(AnalyzerTest, ParseErrorStringFormat) {
    Analyzer analyzer;

    // "filename:lineNum: message" 형식 파싱 테스트를 위해
    // analyze()로 잘못된 스크립트를 분석
    analyzer.analyze(
        "label start:\n"
        "    jump nonexistent\n",
        "file:///test.gyeol"
    );

    auto& diags = analyzer.getDiagnostics();
    // 잘못된 jump target이므로 에러가 있어야 함
    ASSERT_GE(diags.size(), 1u);
    EXPECT_EQ(diags[0].severity, 1); // Error

    // line은 0-based (Parser 1-based에서 변환됨)
    EXPECT_GE(diags[0].line, 0);
}

TEST(AnalyzerTest, DiagnosticsValidScript) {
    Analyzer analyzer;
    analyzer.analyze(
        "label start:\n"
        "    hero \"hello\"\n",
        "file:///test.gyeol"
    );

    // 유효한 스크립트 → 에러 없음
    EXPECT_TRUE(analyzer.getDiagnostics().empty());
}

// ==========================================================================
// LSP Server 테스트
// ==========================================================================

TEST(LspServerTest, Initialize) {
    LspServer server;

    json msg = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"capabilities", json::object()}
        }}
    };

    json response = server.handleMessage(msg);

    EXPECT_EQ(response["id"], 1);
    EXPECT_TRUE(response.contains("result"));
    auto& result = response["result"];
    EXPECT_TRUE(result.contains("capabilities"));
    EXPECT_TRUE(result["capabilities"].contains("textDocumentSync"));
    EXPECT_TRUE(result["capabilities"].contains("completionProvider"));
    EXPECT_TRUE(result["capabilities"].contains("definitionProvider"));
    EXPECT_TRUE(result["capabilities"].contains("hoverProvider"));
    EXPECT_TRUE(result["capabilities"].contains("documentSymbolProvider"));
    EXPECT_EQ(result["serverInfo"]["name"], "GyeolLSP");
}

TEST(LspServerTest, ShutdownAndExit) {
    LspServer server;

    // initialize
    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    EXPECT_FALSE(server.isShutdown());
    EXPECT_FALSE(server.shouldExit());

    // shutdown
    json shutdownResp = server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 2}, {"method", "shutdown"}
    });
    EXPECT_TRUE(server.isShutdown());
    EXPECT_FALSE(server.shouldExit());

    // exit
    server.handleMessage({
        {"jsonrpc", "2.0"}, {"method", "exit"}
    });
    EXPECT_TRUE(server.shouldExit());
}

TEST(LspServerTest, DidOpenPublishesDiagnostics) {
    LspServer server;

    // initialize
    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    // didOpen with valid content
    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    hero \"hello\"\n"}
            }}
        }}
    });

    auto notifs = server.takePendingNotifications();
    // 유효한 스크립트 → 빈 진단 (에러 없음)
    ASSERT_GE(notifs.size(), 1u);
    EXPECT_EQ(notifs[0]["method"], "textDocument/publishDiagnostics");
    EXPECT_EQ(notifs[0]["params"]["uri"], "file:///test.gyeol");
}

TEST(LspServerTest, DidOpenWithErrors) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    // didOpen with invalid jump target
    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    jump nowhere\n"}
            }}
        }}
    });

    auto notifs = server.takePendingNotifications();
    ASSERT_GE(notifs.size(), 1u);
    auto& diags = notifs[0]["params"]["diagnostics"];
    EXPECT_GE(diags.size(), 1u);
}

TEST(LspServerTest, CompletionKeywords) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    // didOpen
    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    \n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // Completion at empty line (should return keywords)
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/completion"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 1}, {"character", 4}}}
        }}
    });

    auto& items = response["result"];
    EXPECT_GT(items.size(), 0u);

    // keyword 중 "label"이 있는지 확인
    bool hasLabel = false;
    for (auto& item : items) {
        if (item["label"] == "label") {
            hasLabel = true;
            break;
        }
    }
    EXPECT_TRUE(hasLabel);
}

TEST(LspServerTest, CompletionLabelsAfterJump) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    jump \nlabel other:\n    \"bye\"\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/completion"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 1}, {"character", 9}}}
        }}
    });

    auto& items = response["result"];
    // "jump " 뒤에서 label 이름 완성
    bool hasOther = false;
    for (auto& item : items) {
        if (item["label"] == "other") {
            hasOther = true;
            break;
        }
    }
    EXPECT_TRUE(hasOther);
}

TEST(LspServerTest, DefinitionLabel) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    jump other\nlabel other:\n    \"bye\"\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // "other" 위에서 Go to Definition (line 1, "other" 시작 위치)
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/definition"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 1}, {"character", 9}}} // "other" 위치
        }}
    });

    auto& result = response["result"];
    // label other: 는 line 2에 있음
    EXPECT_EQ(result["range"]["start"]["line"], 2);
}

TEST(LspServerTest, DefinitionVariable) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    $ myvar = 10\n    $ myvar = 20\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // "myvar" 위에서 Go to Definition (line 2에서 참조)
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/definition"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 2}, {"character", 6}}} // "myvar" 위치
        }}
    });

    auto& result = response["result"];
    // 첫 선언 위치 (line 1)
    EXPECT_EQ(result["range"]["start"]["line"], 1);
}

TEST(LspServerTest, HoverKeyword) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    jump other\nlabel other:\n    \"bye\"\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // "label" 위에서 hover
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/hover"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 0}, {"character", 2}}} // "label" 위
        }}
    });

    auto& result = response["result"];
    EXPECT_TRUE(result.contains("contents"));
    std::string value = result["contents"]["value"].get<std::string>();
    EXPECT_TRUE(value.find("label") != std::string::npos);
}

TEST(LspServerTest, HoverLabel) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    jump other\nlabel other:\n    \"bye\"\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // "other" 위에서 hover (line 1, jump other)
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/hover"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 1}, {"character", 9}}}
        }}
    });

    auto& result = response["result"];
    EXPECT_TRUE(result.contains("contents"));
    std::string value = result["contents"]["value"].get<std::string>();
    EXPECT_TRUE(value.find("other") != std::string::npos);
}

TEST(LspServerTest, HoverLabelWithParams) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    call add\nlabel add(a, b):\n    return a + b\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // "add" 위에서 hover
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/hover"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 1}, {"character", 9}}}
        }}
    });

    auto& result = response["result"];
    std::string value = result["contents"]["value"].get<std::string>();
    EXPECT_TRUE(value.find("add") != std::string::npos);
    EXPECT_TRUE(value.find("a") != std::string::npos);
    EXPECT_TRUE(value.find("b") != std::string::npos);
}

TEST(LspServerTest, DocumentSymbols) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "$ hp = 100\nlabel start:\n    hero \"hi\"\nlabel boss:\n    $ damage = 50\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/documentSymbol"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}}
        }}
    });

    auto& symbols = response["result"];
    ASSERT_GE(symbols.size(), 3u); // 2 labels + 2 vars (hp, damage)

    // label 심볼 확인
    bool hasStart = false, hasBoss = false;
    bool hasHp = false, hasDamage = false;
    for (auto& sym : symbols) {
        std::string name = sym["name"].get<std::string>();
        int kind = sym["kind"].get<int>();
        if (name == "start" && kind == 12) hasStart = true;  // Function
        if (name == "boss" && kind == 12) hasBoss = true;
        if (name == "hp" && kind == 13) hasHp = true;        // Variable
        if (name == "damage" && kind == 13) hasDamage = true;
    }
    EXPECT_TRUE(hasStart);
    EXPECT_TRUE(hasBoss);
    EXPECT_TRUE(hasHp);
    EXPECT_TRUE(hasDamage);
}

TEST(LspServerTest, DidChangeUpdatesDocument) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    // didOpen
    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    hero \"v1\"\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // didChange → 새로운 label 추가
    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didChange"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}, {"version", 2}}},
            {"contentChanges", json::array({
                {{"text", "label start:\n    hero \"v2\"\nlabel newnode:\n    \"new\"\n"}}
            })}
        }}
    });
    server.takePendingNotifications();

    // completion → newnode 포함
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"method", "textDocument/completion"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 0}, {"character", 0}}}
        }}
    });

    auto& items = response["result"];
    bool hasNewNode = false;
    for (auto& item : items) {
        if (item["label"] == "newnode") {
            hasNewNode = true;
            break;
        }
    }
    EXPECT_TRUE(hasNewNode);
}

TEST(LspServerTest, DidCloseRemovesDocument) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // didClose
    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didClose"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}}
        }}
    });

    // 빈 진단 알림이 있어야 함
    auto notifs = server.takePendingNotifications();
    bool hasClearDiag = false;
    for (auto& n : notifs) {
        if (n["method"] == "textDocument/publishDiagnostics" &&
            n["params"]["diagnostics"].empty()) {
            hasClearDiag = true;
        }
    }
    EXPECT_TRUE(hasClearDiag);

    // 닫힌 문서에 대한 completion → 빈 결과
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/completion"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 0}, {"character", 0}}}
        }}
    });
    EXPECT_TRUE(response["result"].empty());
}

TEST(LspServerTest, UnknownMethodError) {
    LspServer server;

    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 99},
        {"method", "unknown/method"}
    });

    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32601);
}

TEST(LspServerTest, HoverOnEmpty) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    hero \"hello\"\n"}
            }}
        }}
    });
    server.takePendingNotifications();

    // 빈 공간에서 hover → null 반환
    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/hover"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 1}, {"character", 0}}} // 들여쓰기 공백
        }}
    });

    EXPECT_TRUE(response["result"].is_null());
}

TEST(LspServerTest, CompletionBuiltinFunctions) {
    LspServer server;

    server.handleMessage({
        {"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"capabilities", json::object()}}}
    });

    server.handleMessage({
        {"jsonrpc", "2.0"},
        {"method", "textDocument/didOpen"},
        {"params", {
            {"textDocument", {
                {"uri", "file:///test.gyeol"},
                {"languageId", "gyeol"},
                {"version", 1},
                {"text", "label start:\n    \n"}
            }}
        }}
    });
    server.takePendingNotifications();

    json response = server.handleMessage({
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "textDocument/completion"},
        {"params", {
            {"textDocument", {{"uri", "file:///test.gyeol"}}},
            {"position", {{"line", 1}, {"character", 4}}}
        }}
    });

    auto& items = response["result"];
    bool hasVisitCount = false;
    bool hasVisited = false;
    for (auto& item : items) {
        if (item["label"] == "visit_count") hasVisitCount = true;
        if (item["label"] == "visited") hasVisited = true;
    }
    EXPECT_TRUE(hasVisitCount);
    EXPECT_TRUE(hasVisited);
}
