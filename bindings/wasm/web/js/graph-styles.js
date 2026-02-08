/**
 * Cytoscape.js stylesheet for Gyeol node graph.
 * Catppuccin Mocha theme colors.
 */
const GRAPH_STYLES = [
    // --- Node base ---
    {
        selector: 'node',
        style: {
            'shape': 'round-rectangle',
            'width': 'label',
            'height': 'label',
            'padding': '12px',
            'background-color': '#181825',
            'border-width': 2,
            'border-color': '#45475a',
            'label': 'data(displayLabel)',
            'text-valign': 'center',
            'text-halign': 'center',
            'color': '#cdd6f4',
            'font-size': '11px',
            'font-family': "'Segoe UI', sans-serif",
            'text-wrap': 'wrap',
            'text-max-width': '180px',
            'text-justification': 'left',
            'min-width': '120px',
            'min-height': '40px',
        }
    },

    // --- Start node ---
    {
        selector: 'node.start',
        style: {
            'border-color': '#89b4fa',
            'border-width': 3,
        }
    },

    // --- Terminal node (no outgoing edges) ---
    {
        selector: 'node.terminal',
        style: {
            'border-color': '#f38ba8',
            'border-width': 2,
            'border-style': 'dashed',
        }
    },

    // --- Function node (has params) ---
    {
        selector: 'node.function',
        style: {
            'border-color': '#cba6f7',
            'border-width': 2,
        }
    },

    // --- Active node (currently playing) ---
    {
        selector: 'node.active',
        style: {
            'border-color': '#a6e3a1',
            'border-width': 3,
            'background-color': '#1a2a1a',
        }
    },

    // --- Visited node ---
    {
        selector: 'node.visited',
        style: {
            'background-color': '#1a1e2a',
        }
    },

    // --- Selected node ---
    {
        selector: 'node:selected',
        style: {
            'border-color': '#f9e2af',
            'border-width': 3,
        }
    },

    // --- Edge base ---
    {
        selector: 'edge',
        style: {
            'width': 2,
            'curve-style': 'bezier',
            'target-arrow-shape': 'triangle',
            'target-arrow-color': '#45475a',
            'line-color': '#45475a',
            'arrow-scale': 0.8,
            'label': 'data(label)',
            'font-size': '9px',
            'color': '#a6adc8',
            'text-rotation': 'autorotate',
            'text-background-color': '#1e1e2e',
            'text-background-opacity': 0.9,
            'text-background-padding': '2px',
            'text-margin-y': -8,
        }
    },

    // --- Jump edge ---
    {
        selector: 'edge.jump',
        style: {
            'line-color': '#89b4fa',
            'target-arrow-color': '#89b4fa',
        }
    },

    // --- Call edge ---
    {
        selector: 'edge.call',
        style: {
            'line-color': '#cba6f7',
            'target-arrow-color': '#cba6f7',
            'width': 3,
            'target-arrow-shape': 'diamond',
        }
    },

    // --- Call with return edge ---
    {
        selector: 'edge.call_return',
        style: {
            'line-color': '#f9e2af',
            'target-arrow-color': '#f9e2af',
            'width': 3,
            'target-arrow-shape': 'diamond',
        }
    },

    // --- Choice edge ---
    {
        selector: 'edge.choice',
        style: {
            'line-color': '#a6e3a1',
            'target-arrow-color': '#a6e3a1',
            'line-style': 'dashed',
            'line-dash-pattern': [6, 3],
        }
    },

    // --- Condition true edge ---
    {
        selector: 'edge.condition_true',
        style: {
            'line-color': '#a6e3a1',
            'target-arrow-color': '#a6e3a1',
            'line-style': 'dotted',
        }
    },

    // --- Condition false edge ---
    {
        selector: 'edge.condition_false',
        style: {
            'line-color': '#f38ba8',
            'target-arrow-color': '#f38ba8',
            'line-style': 'dotted',
        }
    },

    // --- Random edge ---
    {
        selector: 'edge.random',
        style: {
            'line-color': '#fab387',
            'target-arrow-color': '#fab387',
            'line-style': 'dashed',
            'line-dash-pattern': [4, 4],
        }
    },

    // --- Traversed edge (during playback) ---
    {
        selector: 'edge.traversed',
        style: {
            'width': 3,
            'opacity': 1,
        }
    },

    // --- Dim unrelated edges ---
    {
        selector: 'edge',
        style: {
            'opacity': 0.7,
        }
    },
];
