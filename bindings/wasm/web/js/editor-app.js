/**
 * Gyeol Playground — Application Orchestrator
 * Manages text editor, graph view, and story preview.
 */

// ============================================================
// Example scripts
// ============================================================
const EXAMPLES = {
    hello: `label start:
    hero "Hey there! I'm Gyeol."
    hero "Want to go on an adventure?"
    menu:
        "Sure, let's go!" -> accept
        "No thanks." -> decline

label accept:
    hero "Great! Let's head out together!"

label decline:
    hero "Okay... maybe next time."`,

    adventure: `$ courage = 0

label start:
    "You wake up in a dark forest."
    hero "Where am I...?"
    menu:
        "Look around." -> explore
        "Run away." -> flee

label explore:
    hero "I'll be brave and explore."
    $ courage = 1
    jump encounter

label flee:
    hero "Too scary!"
    jump encounter

label encounter:
    "A giant wolf appears!"
    if courage == 1 -> brave else coward

label brave:
    hero "I won't back down!"
    "The wolf, impressed by your courage, steps aside."

label coward:
    hero "Help! Someone save me!"
    "The wolf takes pity and walks away."`,

    variables: `$ hp = 100
$ name = "Hero"
$ has_key = false

label start:
    "Welcome, {name}! Your HP is {hp}."
    menu:
        "Find the key" -> find_key
        "Go to the door" -> door

label find_key:
    "{name} searches the room..."
    $ has_key = true
    $ hp = hp - 10
    "Found a rusty key! (HP: {hp})"
    jump door

label door:
    if has_key == true -> unlock else locked

label unlock:
    "{name} unlocks the door with the key!"
    "Freedom at last!"

label locked:
    "The door is locked. You need a key."
    menu:
        "Search for a key" -> find_key`,

    characters: `character hero:
    displayName: "Aria"
    color: "#4CAF50"

character villain:
    displayName: "Dark Lord"
    color: "#F44336"

character npc:
    displayName: "Old Sage"
    color: "#2196F3"

label start:
    npc "Brave one, the Dark Lord threatens our land."
    hero "I will stop him!"
    villain "You dare challenge me?"
    menu:
        "Fight!" -> battle
        "Negotiate" -> talk

label battle:
    hero "For the people!"
    villain "Foolish mortal!"
    "An epic battle ensues..."
    hero "I... I did it!"

label talk:
    hero "There must be another way."
    villain "Hmph. Perhaps you're wiser than I thought."
    npc "Peace is the greatest victory."`,

    functions: `label start:
    $ greeting = call greet("Hero", 100)
    hero "{greeting}"
    $ farewell = call greet("Traveler", 30)
    hero "{farewell}"

label greet(name, hp):
    $ msg = "{if hp > 50}Welcome, strong {name}!{else}Rest well, {name}.{endif}"
    return msg`
};

// ============================================================
// App State
// ============================================================
let Module = null;
let engine = null;
let isPlaying = false;
let waitingForChoice = false;
let currentView = 'hybrid'; // 'text', 'graph', 'hybrid'
let graphView = null;

// DOM references
const editor = document.getElementById('editor');
const dialogueArea = document.getElementById('dialogue-area');
const choicesContainer = document.getElementById('choices-container');
const errorBar = document.getElementById('error-bar');
const btnCompile = document.getElementById('btn-compile');
const btnNext = document.getElementById('btn-next');
const btnRestart = document.getElementById('btn-restart');
const examplesSelect = document.getElementById('examples');
const statusLeft = document.getElementById('status-left');
const statusNode = document.getElementById('status-node');
const loadingEl = document.getElementById('loading');

// ============================================================
// Initialize
// ============================================================
async function initWasm() {
    try {
        Module = await createGyeolModule();
        engine = new Module.GyeolEngine();
        btnCompile.disabled = false;
        loadingEl.innerHTML = '<span style="color:var(--subtext)">Write a script and click "Compile & Run" to start.</span>';
        statusLeft.textContent = 'Ready';

        // Init graph view
        graphView = new GraphView('graph-container');
        graphView.onNodeSelect = onGraphNodeSelect;

        // Load default example
        editor.value = EXAMPLES.hello;
    } catch (e) {
        loadingEl.innerHTML = `<span style="color:var(--red)">Failed to load WASM: ${e.message}</span>`;
        statusLeft.textContent = 'Error loading WASM';
    }
}

// ============================================================
// View Mode
// ============================================================
function setViewMode(mode) {
    currentView = mode;
    document.body.setAttribute('data-view', mode);

    // Update toggle buttons
    document.querySelectorAll('.view-toggle button').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.view === mode);
    });
}

// ============================================================
// Compile & Run
// ============================================================
function compileAndRun() {
    const source = editor.value;
    if (!source.trim()) return;

    clearError();
    clearDialogue();
    clearChoices();
    isPlaying = false;
    waitingForChoice = false;

    const result = engine.compileAndLoad(source);

    if (!result.success) {
        showErrors(result.errors);
        statusLeft.textContent = 'Compilation failed';
        return;
    }

    if (result.warnings && result.warnings.length > 0) {
        showWarnings(result.warnings);
    }

    // Update graph
    updateGraph();

    isPlaying = true;
    btnNext.disabled = false;
    btnRestart.disabled = false;
    statusLeft.textContent = 'Playing...';

    addSystemLine('Story started');
    advanceStory();
}

/** 그래프 업데이트 (컴파일 후) */
function updateGraph() {
    if (!engine || !graphView) return;
    try {
        const graphData = engine.getGraphData();
        graphView.render(graphData);
        updateNodeStatus();
    } catch (e) {
        console.warn('Graph update failed:', e);
    }
}

/** 그래프에 현재 노드 하이라이트 */
function updateNodeStatus() {
    if (!engine || !graphView) return;
    try {
        const nodeName = engine.getCurrentNodeName();
        if (nodeName) {
            graphView.highlightNode(nodeName);
            if (statusNode) statusNode.textContent = nodeName;
        }
    } catch (e) {
        // ignore — runner may not be started
    }
}

// ============================================================
// Graph ↔ Preview interaction
// ============================================================
function onGraphNodeSelect(nodeName) {
    if (!engine) return;

    // Re-start from selected node
    clearDialogue();
    clearChoices();
    clearError();
    waitingForChoice = false;

    const ok = engine.startFromNode(nodeName);
    if (!ok) {
        addSystemLine('Failed to start from node: ' + nodeName);
        return;
    }

    isPlaying = true;
    btnNext.disabled = false;
    btnRestart.disabled = false;
    statusLeft.textContent = 'Playing from ' + nodeName;

    // Clear previous highlights and set new
    graphView.clearHighlights();
    graphView.highlightNode(nodeName);

    addSystemLine('Playing from: ' + nodeName);
    advanceStory();
}

// ============================================================
// Story advancement
// ============================================================
function advanceStory() {
    if (!engine || !isPlaying || waitingForChoice) return;

    const result = engine.step();
    updateNodeStatus();

    switch (result.type) {
        case 'LINE':
            addDialogueLine(result.character, result.text, result.tags);
            break;

        case 'CHOICES':
            waitingForChoice = true;
            btnNext.disabled = true;
            showChoices(result.choices);
            return;

        case 'COMMAND':
            addCommandLine(result.commandType, result.params);
            advanceStory();
            return;

        case 'END':
            isPlaying = false;
            btnNext.disabled = true;
            addEndLine();
            statusLeft.textContent = 'Story ended';
            return;
    }
}

function selectChoice(index) {
    if (!engine || !waitingForChoice) return;

    clearChoices();
    waitingForChoice = false;
    btnNext.disabled = false;

    engine.choose(index);
    advanceStory();
}

function restart() {
    if (graphView) graphView.clearHighlights();
    compileAndRun();
}

// ============================================================
// UI helpers
// ============================================================
function addDialogueLine(character, text, tags) {
    const div = document.createElement('div');
    div.className = 'dialogue-line' + (character ? '' : ' narration');

    if (character) {
        const charSpan = document.createElement('span');
        charSpan.className = 'character';
        charSpan.textContent = `[${character}]`;
        div.appendChild(charSpan);
    }

    const textSpan = document.createElement('span');
    textSpan.className = 'text';
    textSpan.textContent = text;
    div.appendChild(textSpan);

    if (tags && tags.length > 0) {
        const tagSpan = document.createElement('span');
        tagSpan.style.cssText = 'color:var(--teal); font-size:11px; margin-left:8px; opacity:0.6;';
        tagSpan.textContent = tags.map(t => `#${t.key}${t.value ? ':' + t.value : ''}`).join(' ');
        div.appendChild(tagSpan);
    }

    dialogueArea.appendChild(div);
    dialogueArea.scrollTop = dialogueArea.scrollHeight;
}

function addCommandLine(type, params) {
    const div = document.createElement('div');
    div.className = 'dialogue-line command';
    div.textContent = `@ ${type} ${params.join(' ')}`;
    dialogueArea.appendChild(div);
    dialogueArea.scrollTop = dialogueArea.scrollHeight;
}

function addSystemLine(text) {
    const div = document.createElement('div');
    div.className = 'dialogue-line system';
    div.textContent = text;
    dialogueArea.appendChild(div);
    dialogueArea.scrollTop = dialogueArea.scrollHeight;
}

function addEndLine() {
    const div = document.createElement('div');
    div.className = 'dialogue-line end-marker';
    div.textContent = '--- END ---';
    dialogueArea.appendChild(div);
    dialogueArea.scrollTop = dialogueArea.scrollHeight;
}

function showChoices(choices) {
    choicesContainer.innerHTML = '';
    for (let i = 0; i < choices.length; i++) {
        const btn = document.createElement('button');
        btn.className = 'choice-btn';
        btn.innerHTML = `<span class="choice-index">[${i + 1}]</span>${escapeHtml(choices[i].text)}`;
        btn.addEventListener('click', () => selectChoice(choices[i].index));
        choicesContainer.appendChild(btn);
    }
}

function clearChoices() { choicesContainer.innerHTML = ''; }
function clearDialogue() { dialogueArea.innerHTML = ''; }

function showErrors(errors) {
    errorBar.innerHTML = errors.map(e => `<div class="error">${escapeHtml(e)}</div>`).join('');
    errorBar.classList.add('visible');
}

function showWarnings(warnings) {
    const html = warnings.map(w => `<div class="warning">${escapeHtml(w)}</div>`).join('');
    errorBar.innerHTML += html;
    if (warnings.length > 0) errorBar.classList.add('visible');
}

function clearError() {
    errorBar.innerHTML = '';
    errorBar.classList.remove('visible');
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ============================================================
// Event handlers
// ============================================================
btnCompile.addEventListener('click', compileAndRun);
btnNext.addEventListener('click', advanceStory);
btnRestart.addEventListener('click', restart);

examplesSelect.addEventListener('change', () => {
    const key = examplesSelect.value;
    if (key && EXAMPLES[key]) {
        editor.value = EXAMPLES[key];
        examplesSelect.value = '';
    }
});

// View mode toggle
document.addEventListener('click', (e) => {
    if (e.target.closest('.view-toggle button')) {
        const mode = e.target.dataset.view;
        if (mode) setViewMode(mode);
    }
});

// Graph controls
document.addEventListener('click', (e) => {
    const btn = e.target.closest('.graph-controls button');
    if (!btn || !graphView) return;
    const action = btn.dataset.action;
    if (action === 'zoom-in') graphView.zoomIn();
    else if (action === 'zoom-out') graphView.zoomOut();
    else if (action === 'fit') graphView.fitAll();
});

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
    if (e.ctrlKey && e.key === 'Enter') {
        e.preventDefault();
        compileAndRun();
    }
    if ((e.key === ' ' || e.key === 'Enter') && document.activeElement !== editor && !waitingForChoice) {
        e.preventDefault();
        advanceStory();
    }
    if (waitingForChoice && e.key >= '1' && e.key <= '9') {
        const idx = parseInt(e.key) - 1;
        const btns = choicesContainer.querySelectorAll('.choice-btn');
        if (idx < btns.length) {
            btns[idx].click();
        }
    }
});

// Tab key in editor
editor.addEventListener('keydown', (e) => {
    if (e.key === 'Tab') {
        e.preventDefault();
        const start = editor.selectionStart;
        const end = editor.selectionEnd;
        editor.value = editor.value.substring(0, start) + '    ' + editor.value.substring(end);
        editor.selectionStart = editor.selectionEnd = start + 4;
    }
});

// Set initial view mode
setViewMode('hybrid');
