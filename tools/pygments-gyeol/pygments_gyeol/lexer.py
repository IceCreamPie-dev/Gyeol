"""
Pygments lexer for the Gyeol (.gyeol) interactive storytelling language.

Gyeol uses indent-based syntax with labels, dialogue, menus, variables,
conditions, and engine commands.
"""

from pygments.lexer import RegexLexer, bygroups, include, default
from pygments.token import (
    Comment,
    Keyword,
    Name,
    Number,
    Operator,
    Punctuation,
    String,
    Text,
    Whitespace,
)


class GyeolLexer(RegexLexer):
    """Pygments lexer for the Gyeol scripting language."""

    name = "Gyeol"
    aliases = ["gyeol"]
    filenames = ["*.gyeol"]
    mimetypes = ["text/x-gyeol"]

    tokens = {
        "root": [
            # Comments
            (r"^\s*#.*$", Comment.Single),

            # Import
            (r"(^\s*)(import)(\s+)", bygroups(Whitespace, Keyword.Namespace, Whitespace), "import-path"),

            # Character definition block
            (r"(^\s*)(character)(\s+)(\w+)(\s*)(:)",
             bygroups(Whitespace, Keyword.Declaration, Whitespace, Name.Class, Whitespace, Punctuation)),

            # Character property (inside character block)
            (r"(^\s+)(\w+)(\s*)(:)(\s*)(\"[^\"]*\")",
             bygroups(Whitespace, Name.Attribute, Whitespace, Punctuation, Whitespace, String.Double)),
            (r"(^\s+)(\w+)(\s*)(:)(\s*)(\S+)",
             bygroups(Whitespace, Name.Attribute, Whitespace, Punctuation, Whitespace, String)),

            # Label with optional params and tags
            (r"(^\s*)(label)(\s+)(\w+)",
             bygroups(Whitespace, Keyword.Declaration, Whitespace, Name.Function),
             "label-rest"),

            # Variable assignment: $ var = ...
            (r"(^\s*)(\$)(\s+)(\w+)(\s*)([\+\-]?=)",
             bygroups(Whitespace, Punctuation, Whitespace, Name.Variable, Whitespace, Operator),
             "expression"),

            # Menu keyword
            (r"(^\s*)(menu)(\s*)(:)",
             bygroups(Whitespace, Keyword, Whitespace, Punctuation)),

            # Random keyword
            (r"(^\s*)(random)(\s*)(:)",
             bygroups(Whitespace, Keyword, Whitespace, Punctuation)),

            # Return statement
            (r"(^\s*)(return)\b",
             bygroups(Whitespace, Keyword),
             "expression"),

            # Call statement (standalone)
            (r"(^\s*)(call)(\s+)(\w+)",
             bygroups(Whitespace, Keyword, Whitespace, Name.Function),
             "call-args"),

            # Jump statement
            (r"(^\s*)(jump)(\s+)(\w+)",
             bygroups(Whitespace, Keyword, Whitespace, Name.Function)),

            # If / elif / else
            (r"(^\s*)(if|elif)\b",
             bygroups(Whitespace, Keyword),
             "condition"),
            (r"(^\s*)(else)(\s*)(->)(\s*)(\w+)",
             bygroups(Whitespace, Keyword, Whitespace, Operator, Whitespace, Name.Function)),

            # Random weight line: 50 -> node
            (r"(^\s*)(\d+)(\s*)(->)(\s*)(\w+)",
             bygroups(Whitespace, Number.Integer, Whitespace, Operator, Whitespace, Name.Function)),
            # Default weight: -> node
            (r"(^\s*)(->)(\s*)(\w+)",
             bygroups(Whitespace, Operator, Whitespace, Name.Function)),

            # Command: @ type params
            (r"(^\s*)(@)(\s*)(\w+)",
             bygroups(Whitespace, Punctuation, Whitespace, Name.Builtin),
             "command-params"),

            # Choice line: "text" -> node [if cond] [#modifier]
            (r'(^\s*)(")', bygroups(Whitespace, String.Double), "choice-or-narration"),

            # Dialogue: character "text"
            (r"(^\s*)(\w+)(\s+)(\")",
             bygroups(Whitespace, Name.Class, Whitespace, String.Double),
             "dialogue-text"),

            # Fallthrough
            (r"\s+", Whitespace),
            (r".", Text),
        ],

        "import-path": [
            (r'"[^"]*"', String.Double),
            (r"\s*$", Whitespace, "#pop"),
        ],

        "label-rest": [
            # Parameters
            (r"\(", Punctuation, "label-params"),
            # Node tags: #key or #key=value
            (r"(#)(\w+)(=)(\w+)",
             bygroups(Punctuation, Name.Decorator, Operator, String)),
            (r"(#)(\w+)",
             bygroups(Punctuation, Name.Decorator)),
            (r":", Punctuation, "#pop"),
            (r"\s+", Whitespace),
            default("#pop"),
        ],

        "label-params": [
            (r"\w+", Name.Variable),
            (r",", Punctuation),
            (r"\s+", Whitespace),
            (r"\)", Punctuation, "#pop"),
        ],

        "expression": [
            (r"\s*$", Whitespace, "#pop"),
            (r"(call)(\s+)(\w+)",
             bygroups(Keyword, Whitespace, Name.Function)),
            (r"(visit_count|visited|list_contains|list_length)\b", Name.Builtin),
            (r"\b(true|false)\b", Keyword.Constant),
            (r"\b(and|or|not)\b", Operator.Word),
            (r"(==|!=|>=|<=|>|<)", Operator),
            (r"(\+|-|\*|/|%)", Operator),
            (r"\d+\.\d+", Number.Float),
            (r"\d+", Number.Integer),
            (r'"', String.Double, "string-content"),
            (r"\(", Punctuation),
            (r"\)", Punctuation),
            (r",", Punctuation),
            (r"\w+", Name.Variable),
            (r"\s+", Whitespace),
        ],

        "condition": [
            (r"\s*$", Whitespace, "#pop"),
            (r"(->)(\s*)(\w+)",
             bygroups(Operator, Whitespace, Name.Function)),
            (r"\b(else)\b", Keyword),
            (r"(visit_count|visited|list_contains|list_length)\b", Name.Builtin),
            (r"\b(true|false)\b", Keyword.Constant),
            (r"\b(and|or|not)\b", Operator.Word),
            (r"(==|!=|>=|<=|>|<)", Operator),
            (r"(\+|-|\*|/|%)", Operator),
            (r"\d+\.\d+", Number.Float),
            (r"\d+", Number.Integer),
            (r'"', String.Double, "string-content"),
            (r"\(", Punctuation),
            (r"\)", Punctuation),
            (r"\w+", Name.Variable),
            (r"\s+", Whitespace),
        ],

        "call-args": [
            (r"\s*$", Whitespace, "#pop"),
            (r"\(", Punctuation, "expression"),
            (r"\s+", Whitespace),
        ],

        "command-params": [
            (r"\s*$", Whitespace, "#pop"),
            (r'"[^"]*"', String.Double),
            (r"\d+\.\d+", Number.Float),
            (r"\d+", Number.Integer),
            (r"\w+", Name.Other),
            (r"\s+", Whitespace),
        ],

        "choice-or-narration": [
            # String content with interpolation
            (r"\{", String.Interpol, "interpolation"),
            (r'[^"\\{]+', String.Double),
            (r"\\.", String.Escape),
            (r'"', String.Double, "after-string"),
        ],

        "dialogue-text": [
            (r"\{", String.Interpol, "interpolation"),
            (r'[^"\\{]+', String.Double),
            (r"\\.", String.Escape),
            (r'"', String.Double, "after-dialogue"),
        ],

        "after-string": [
            # Choice arrow: -> target
            (r"(\s*)(->)(\s*)(\w+)",
             bygroups(Whitespace, Operator, Whitespace, Name.Function)),
            # Condition: if var
            (r"(\s+)(if)\b", bygroups(Whitespace, Keyword)),
            # Choice modifier: #once, #sticky, #fallback
            (r"(\s*)(#)(once|sticky|fallback)\b",
             bygroups(Whitespace, Punctuation, Name.Decorator)),
            # Dialogue tags: #key:value or #key
            (r"(\s*)(#)(\w+)(:)(\S+)",
             bygroups(Whitespace, Punctuation, Name.Decorator, Punctuation, String)),
            (r"(\s*)(#)(\w+)",
             bygroups(Whitespace, Punctuation, Name.Decorator)),
            (r"\b(and|or|not)\b", Operator.Word),
            (r"(==|!=|>=|<=|>|<)", Operator),
            (r"\b(true|false)\b", Keyword.Constant),
            (r"\d+", Number.Integer),
            (r"\w+", Name.Variable),
            (r"\s*$", Whitespace, "#pop:2"),
            (r"\s+", Whitespace),
        ],

        "after-dialogue": [
            # Dialogue tags: #key:value or #key
            (r"(\s*)(#)(\w+)(:)(\S+)",
             bygroups(Whitespace, Punctuation, Name.Decorator, Punctuation, String)),
            (r"(\s*)(#)(\w+)",
             bygroups(Whitespace, Punctuation, Name.Decorator)),
            (r"\s*$", Whitespace, "#pop:2"),
            (r"\s+", Whitespace),
        ],

        "string-content": [
            (r"\{", String.Interpol, "interpolation"),
            (r'[^"\\{]+', String.Double),
            (r"\\.", String.Escape),
            (r'"', String.Double, "#pop"),
        ],

        "interpolation": [
            (r"\b(if|else|endif)\b", Keyword),
            (r"(visit_count|visited)\b", Name.Builtin),
            (r"\b(and|or|not)\b", Operator.Word),
            (r"(==|!=|>=|<=|>|<)", Operator),
            (r"\d+", Number.Integer),
            (r"\w+", Name.Variable),
            (r"\(", Punctuation),
            (r"\)", Punctuation),
            (r"\}", String.Interpol, "#pop"),
            (r"\s+", Whitespace),
            (r".", String.Interpol),
        ],
    }
