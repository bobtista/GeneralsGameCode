"""
Convert leading spaces to tabs using tree-sitter for accurate C++ parsing.

Uses the tree-sitter C++ parser to determine the correct indentation depth
of each line, then replaces leading whitespace with the correct number of tabs.

Only converts lines where the tool is confident about the correct depth.
Lines inside block comments, continuation lines, and ambiguous cases are
skipped and logged.

Requirements:
    pip install tree-sitter tree-sitter-cpp

Usage:
    python convert_leading_spaces_to_tabs.py [--dry-run] [--verbose]
"""

import argparse
import glob
import os
import re

import tree_sitter_cpp as tscpp
from tree_sitter import Language, Parser


CPP_LANGUAGE = Language(tscpp.language())


# Macros that confuse tree-sitter's C++ parser. These are expanded in the
# source text before parsing (but not in the output). Replacements are
# same-length to preserve byte offsets between the parsed and original text.
MACRO_EXPANSIONS = [
    (re.compile(rb'\bCALLBACK\b'),    b'        '),
    (re.compile(rb'\bGCALL\b'),       b'     '),
    (re.compile(rb'\bWINAPI\b'),      b'      '),
    (re.compile(rb'\bIN\b'),          b'  '),
    (re.compile(rb'\bOUT\b'),         b'   '),
    (re.compile(rb'\bRO\b'),          b'  '),
    (re.compile(rb'\bW3DNEW\b'),      b'new   '),
    (re.compile(rb'\bNEW\b'),         b'new'),
    (re.compile(rb'\b__RPC_FAR\b'),   b'         '),
    (re.compile(rb'\b__RPC_STUB\b'),  b'          '),
    (re.compile(rb'\b__asm\b'),       b'     '),
    (re.compile(rb'\b_asm\b'),        b'    '),
]


def preprocess_for_parsing(code_bytes):
    """Apply macro expansions to help tree-sitter parse the code."""
    result = code_bytes
    for pattern, replacement in MACRO_EXPANSIONS:
        result = pattern.sub(replacement, result)
    return result


# Node types that create an indentation level for their children.
SCOPE_CREATING_TYPES = frozenset({
    'compound_statement',
    'field_declaration_list',
    'declaration_list',      # namespace body
    'enumerator_list',
    'initializer_list',
    'case_statement',
})

# Control-flow statements whose braceless body should be indented one level.
BRACELESS_BODY_PARENTS = frozenset({
    'if_statement',
    'while_statement',
    'for_statement',
    'do_statement',
    'else_clause',
})


def get_indent_depth(node):
    """Walk up the AST and count scope-creating ancestors to determine indent depth."""
    depth = 0
    current = node.parent
    prev = node

    while current is not None:
        if current.type in SCOPE_CREATING_TYPES:
            # Special case: a compound_statement that is a direct child of
            # case_statement does not add an extra indent level. The case_statement
            # already provides the indent, and the braces are at the case level.
            if (current.type == 'compound_statement'
                    and current.parent is not None
                    and current.parent.type == 'case_statement'):
                pass  # don't increment
            else:
                depth += 1
        elif current.type in BRACELESS_BODY_PARENTS:
            # If prev is the body/consequence of a braceless control-flow
            # statement, it needs one extra indent level.
            body_field = (current.child_by_field_name('consequence')
                          or current.child_by_field_name('body')
                          or current.child_by_field_name('alternative'))
            if (body_field is not None
                    and body_field.id == prev.id
                    and body_field.type != 'compound_statement'):
                depth += 1
        prev = current
        current = current.parent

    return depth


def is_continuation_line(node, line_num):
    """Check if this line is a continuation of a multi-line statement.

    A continuation line is one where the innermost meaningful AST node
    started on a previous line. We skip these because their alignment
    is a style choice we don't want to change in this PR.
    """
    if node is None:
        return False

    # Walk up to find the nearest statement-level node
    current = node
    while current is not None:
        # If this node starts on our line, it's not a continuation
        if current.start_point[0] == line_num:
            current = current.parent
            continue

        # If the node started on a previous line and is a statement-level
        # construct, this is a continuation line
        if current.start_point[0] < line_num:
            if current.type in (
                'declaration', 'field_declaration', 'expression_statement',
                'return_statement', 'init_declarator', 'argument_list',
                'parameter_list', 'condition_clause', 'call_expression',
                'binary_expression', 'assignment_expression',
                'function_declarator', 'template_argument_list',
                'field_initializer_list', 'field_initializer',
                'base_class_clause', 'initializer_pair',
            ):
                return True
        current = current.parent

    return False





def process_file(filepath, dry_run=False, verbose=False):
    """Process a single file, converting leading spaces to tabs.

    Returns (changed, skipped, total_lines) tuple.
    """
    parser = Parser(CPP_LANGUAGE)

    try:
        with open(filepath, 'r', encoding='cp1252') as f:
            content = f.read()
    except (UnicodeDecodeError, OSError):
        return 0, 0, 0

    lines = content.split('\n')
    # Preserve original line endings
    if content.endswith('\n'):
        lines = lines[:-1]  # split adds an empty string after trailing \n

    code_bytes = content.encode('utf-8')
    parse_bytes = preprocess_for_parsing(code_bytes)
    tree = parser.parse(parse_bytes)

    # If the file has excessive parse errors, the AST is unreliable - skip it.
    # Only count top-level errors (not deeply nested ones from macro expansions).
    error_count = sum(1 for child in tree.root_node.children if child.type == 'ERROR')
    if error_count > 10:
        if verbose:
            print(f"  SKIP file (too many top-level parse errors: {error_count})")
        return 0, 0, len(lines)

    new_lines = []
    changed = 0
    skipped = 0
    in_block_comment = False
    in_macro_continuation = False
    in_asm_block = False
    asm_brace_depth = 0
    asm_pending = False
    pp_depth = 0

    for line_idx, line in enumerate(lines):
        # Track block comments manually for reliability
        stripped = line.lstrip()
        line_continues_macro = line.rstrip().endswith('\\')

        if in_block_comment:
            # Inside a block comment - don't touch
            new_lines.append(line)
            if '*/' in line:
                in_block_comment = False
            continue

        if stripped.startswith('/*'):
            if '*/' not in stripped or stripped.index('*/') < stripped.index('/*') + 2:
                in_block_comment = '*/' not in stripped[2:]
            new_lines.append(line)
            continue

        # Inline assembly blocks - don't touch (uses spaces for alignment)
        if in_asm_block:
            new_lines.append(line)
            asm_brace_depth += stripped.count('{') - stripped.count('}')
            if asm_brace_depth <= 0:
                in_asm_block = False
            continue

        # Check for pending asm block (keyword was on previous line)
        if asm_pending:
            asm_pending = False
            if stripped.startswith('{'):
                in_asm_block = True
                asm_brace_depth = stripped.count('{') - stripped.count('}')
                if asm_brace_depth <= 0:
                    in_asm_block = False
                new_lines.append(line)
                continue

        # Multi-line macro continuations - don't touch
        if in_macro_continuation:
            new_lines.append(line)
            in_macro_continuation = line_continues_macro
            continue

        # Blank lines - preserve as-is
        if stripped == '':
            new_lines.append(line)
            continue

        # Preprocessor directives - always column 0
        if stripped.startswith('#'):
            # Track preprocessor nesting depth
            directive = stripped.lstrip('#').lstrip()
            if directive.startswith(('if ', 'if\t', 'ifdef ', 'ifdef\t',
                                     'ifndef ', 'ifndef\t')):
                pp_depth += 1
            elif directive.startswith('endif'):
                pp_depth = max(0, pp_depth - 1)
            new_lines.append(line)
            in_macro_continuation = line_continues_macro
            continue

        # Check for inline assembly block start
        if re.search(r'\b__?asm\b', stripped) and not stripped.startswith('//'):
            if '{' in stripped:
                in_asm_block = True
                asm_brace_depth = stripped.count('{') - stripped.count('}')
                if asm_brace_depth <= 0:
                    in_asm_block = False
            else:
                asm_pending = True
            new_lines.append(line)
            continue

        # Check if the line already uses tabs (no leading spaces)
        leading_ws = line[:len(line) - len(line.lstrip())]
        if ' ' not in leading_ws:
            # Already all tabs (or no indentation) - keep as-is
            new_lines.append(line)
            continue

        # Tabs-then-spaces: the spaces are for visual alignment, not indentation.
        # E.g. a continuation comment aligned under a previous line's comment.
        # Require 2+ trailing spaces — a single space is almost always a stray,
        # not intentional alignment.
        if leading_ws[0] == '\t':
            first_space = leading_ws.find(' ')
            trailing_spaces = len(leading_ws) - first_space if first_space > 0 else 0
            if first_space > 0 and '\t' not in leading_ws[first_space:] and trailing_spaces >= 2:
                new_lines.append(line)
                skipped += 1
                if verbose:
                    print(f"  SKIP tab-align L{line_idx+1}: {line.rstrip()[:80]}")
                continue

        # Mixed whitespace with spaces before tabs (or trailing single stray
        # space): if the spaces are minor (1-3), they're stray characters and
        # tab count is the real depth. If spaces are significant (4+), fall
        # through to tree-sitter.
        tab_count = leading_ws.count('\t')
        if tab_count > 0:
            first_tab_pos = leading_ws.index('\t')
            if first_tab_pos < 4:
                new_line = '\t' * tab_count + stripped
                if new_line != line:
                    changed += 1
                    if verbose:
                        print(f"  CHANGE L{line_idx+1} (mixed->tabs={tab_count}): '{leading_ws}' -> '{chr(9)*tab_count}'")
                new_lines.append(new_line)
                continue

        # This line has pure leading spaces.
        # Use tree-sitter to determine the correct depth.
        # Find the AST node at the first non-whitespace character.
        col = len(leading_ws)
        node = tree.root_node.descendant_for_point_range(
            (line_idx, col), (line_idx, col)
        )

        if node is None:
            new_lines.append(line)
            skipped += 1
            continue

        # Skip if the node or any ancestor is an ERROR node (parse failure)
        error_ancestor = False
        check = node
        while check is not None:
            if check.type == 'ERROR':
                error_ancestor = True
                break
            check = check.parent
        if error_ancestor:
            new_lines.append(line)
            skipped += 1
            if verbose:
                print(f"  SKIP parse error L{line_idx+1}: {line.rstrip()[:80]}")
            continue

        # For comment nodes, we can reindent // line comments but
        # should skip /* */ block comments (their internal formatting
        # is intentional). Block comments are already handled by the
        # in_block_comment tracking above, so single-line comments
        # that tree-sitter reports as 'comment' are fine to reindent.
        # However, single-line /* */ comments on their own line are also
        # fine to reindent.

        # Skip continuation lines
        if is_continuation_line(node, line_idx):
            new_lines.append(line)
            skipped += 1
            if verbose:
                print(f"  SKIP continuation L{line_idx+1}: {line.rstrip()[:80]}")
            continue

        # Calculate expected depth
        depth = get_indent_depth(node)

        # Special handling: the opening/closing braces of a compound_statement
        # should be at the parent's depth, not the compound_statement's depth.
        if node.type in ('{', '}'):
            parent = node.parent
            if parent is not None and parent.type in SCOPE_CREATING_TYPES:
                # For a compound_statement inside case_statement, braces should
                # be at the case label level (case_statement's own depth)
                if (parent.type == 'compound_statement'
                        and parent.parent is not None
                        and parent.parent.type == 'case_statement'):
                    depth = get_indent_depth(parent.parent)
                else:
                    depth = get_indent_depth(parent)

        # Special handling: case/default labels - they are children of
        # case_statement but should be at the case_statement's depth
        # (the 'case' keyword itself)
        if node.type in ('case', 'default'):
            parent = node.parent
            if parent is not None and parent.type == 'case_statement':
                depth = get_indent_depth(parent)

        # Special handling: access specifiers (public/private/protected)
        # These sit at the class brace level, not indented inside the class body.
        if node.type in ('public', 'private', 'protected'):
            access_spec = node.parent  # access_specifier node
            if access_spec is not None and access_spec.type == 'access_specifier':
                field_list = access_spec.parent  # field_declaration_list
                if field_list is not None:
                    depth = get_indent_depth(field_list)

        # Special handling: the colon after access specifier
        if node.type == ':':
            parent = node.parent
            if parent is not None and parent.type == 'access_specifier':
                field_list = parent.parent
                if field_list is not None:
                    depth = get_indent_depth(field_list)

        # Sanity check: if the line had significant indentation but we
        # computed depth 0, something is likely wrong (parse errors nearby,
        # inline assembly corrupting the AST, etc). Skip to be safe.
        # Also skip depth-0 lines inside preprocessor blocks — the codebase
        # convention is to indent C++ code inside #if/#ifdef blocks, but
        # tree-sitter doesn't model preprocessor nesting.
        space_count = len(leading_ws)
        if depth == 0 and space_count > 0 and pp_depth > 0:
            new_lines.append(line)
            skipped += 1
            if verbose:
                print(f"  SKIP pp-indent L{line_idx+1}: depth=0 but pp_depth={pp_depth}: {line.rstrip()[:80]}")
            continue
        if depth == 0 and space_count >= 4:
            new_lines.append(line)
            skipped += 1
            if verbose:
                print(f"  SKIP sanity L{line_idx+1}: depth=0 but {space_count} spaces: {line.rstrip()[:80]}")
            continue

        # Build the new line: depth tabs + original content (no leading whitespace)
        new_line = '\t' * depth + stripped
        if new_line != line:
            changed += 1
            if verbose:
                print(f"  CHANGE L{line_idx+1} (depth={depth}): '{leading_ws}' -> '{chr(9)*depth}'")
        new_lines.append(new_line)

        # Track mid-line block comment opens (e.g. /**< trailing doc comments)
        last_open = stripped.rfind('/*')
        if last_open >= 0:
            after_open = stripped[last_open + 2:]
            if '*/' not in after_open:
                in_block_comment = True

    if changed > 0 and not dry_run:
        with open(filepath, 'w', encoding='cp1252') as f:
            f.write('\n'.join(new_lines))
            if content.endswith('\n'):
                f.write('\n')

    return changed, skipped, len(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Convert leading spaces to tabs using tree-sitter C++ parsing.'
    )
    parser.add_argument('--dry-run', action='store_true',
                        help='Show what would change without modifying files')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Print details about each changed/skipped line')
    parser.add_argument('--file', type=str,
                        help='Process a single file instead of the whole codebase')
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.normpath(os.path.join(script_dir, '..', '..'))

    if args.file:
        file_list = [args.file]
    else:
        top_dirs = [
            os.path.join(root_dir, 'Core'),
            os.path.join(root_dir, 'Generals'),
            os.path.join(root_dir, 'GeneralsMD'),
            os.path.join(root_dir, 'Dependencies', 'Utility'),
        ]
        file_list = []
        for ext in ['*.cpp', '*.h', '*.inl']:
            for top_dir in top_dirs:
                file_list.extend(
                    glob.glob(os.path.join(top_dir, '**', ext), recursive=True)
                )

    total_changed = 0
    total_skipped = 0
    total_files_modified = 0

    for filepath in sorted(file_list):
        changed, skipped, total_lines = process_file(
            filepath, dry_run=args.dry_run, verbose=args.verbose
        )
        if changed > 0:
            total_files_modified += 1
            rel = os.path.relpath(filepath, root_dir)
            action = "Would change" if args.dry_run else "Changed"
            print(f"{action} {changed} lines in {rel}")
        total_changed += changed
        total_skipped += skipped

    action = "Would change" if args.dry_run else "Changed"
    print(f"\n{action} {total_changed} lines across {total_files_modified} files")
    print(f"Skipped {total_skipped} lines (continuations, ambiguous)")


if __name__ == '__main__':
    main()
