/**
 * Gyeol Graph View — Cytoscape.js 기반 노드 그래프 패널
 */
class GraphView {
    constructor(containerId) {
        this.containerId = containerId;
        this.cy = null;
        this.tooltip = null;
        this.onNodeSelect = null; // callback(nodeName)
        this.visitedNodes = new Set();
        this._initTooltip();
    }

    /** Cytoscape 인스턴스 초기화 */
    init() {
        this.cy = cytoscape({
            container: document.getElementById(this.containerId),
            style: GRAPH_STYLES,
            layout: { name: 'preset' },
            minZoom: 0.2,
            maxZoom: 3,
            wheelSensitivity: 0.3,
        });

        // 노드 클릭 → 선택 콜백
        this.cy.on('tap', 'node', (evt) => {
            const nodeName = evt.target.data('nodeName');
            if (this.onNodeSelect) {
                this.onNodeSelect(nodeName);
            }
        });

        // 노드 hover → 툴팁
        this.cy.on('mouseover', 'node', (evt) => {
            this._showTooltip(evt.target, evt.renderedPosition);
        });
        this.cy.on('mouseout', 'node', () => {
            this._hideTooltip();
        });
        this.cy.on('drag', 'node', () => {
            this._hideTooltip();
        });
    }

    /** 그래프 데이터로 렌더링 */
    render(graphData) {
        if (!this.cy) this.init();

        this.cy.elements().remove();
        this.visitedNodes.clear();

        const elements = this._convertToElements(graphData);
        this.cy.add(elements);

        // 노드 클래스 적용
        this._applyNodeClasses(graphData);

        // dagre 레이아웃 실행
        this.cy.layout({
            name: 'dagre',
            rankDir: 'TB',
            nodeSep: 40,
            rankSep: 60,
            edgeSep: 20,
            animate: false,
            padding: 30,
        }).run();

        this.cy.fit(undefined, 40);
    }

    /** 현재 활성 노드 하이라이트 */
    highlightNode(nodeName) {
        // 이전 active 제거
        this.cy.nodes('.active').removeClass('active');

        // 새 active 적용
        const node = this.cy.getElementById('node-' + nodeName);
        if (node.length) {
            node.addClass('active');
            this.visitedNodes.add(nodeName);
            node.addClass('visited');

            // 뷰를 현재 노드로 이동 (부드럽게)
            this.cy.animate({
                center: { eles: node },
                duration: 300,
            });
        }
    }

    /** 모든 하이라이트 초기화 */
    clearHighlights() {
        if (!this.cy) return;
        this.cy.nodes().removeClass('active visited');
        this.visitedNodes.clear();
    }

    /** 줌 제어 */
    zoomIn() {
        if (!this.cy) return;
        this.cy.zoom({ level: this.cy.zoom() * 1.3, renderedPosition: this._center() });
    }

    zoomOut() {
        if (!this.cy) return;
        this.cy.zoom({ level: this.cy.zoom() / 1.3, renderedPosition: this._center() });
    }

    fitAll() {
        if (!this.cy) return;
        this.cy.fit(undefined, 40);
    }

    // ===========================================
    // Private methods
    // ===========================================

    /** GraphData → Cytoscape elements 변환 */
    _convertToElements(graphData) {
        const elements = [];
        const nodeNames = new Set(graphData.nodes.map(n => n.name));
        const outgoing = new Set();

        // Edges — 유효한 타겟만
        for (const edge of graphData.edges) {
            if (!nodeNames.has(edge.to)) continue;
            outgoing.add(edge.from);
            elements.push({
                group: 'edges',
                data: {
                    id: `edge-${edge.from}-${edge.to}-${edge.type}-${elements.length}`,
                    source: 'node-' + edge.from,
                    target: 'node-' + edge.to,
                    label: edge.label || '',
                    edgeType: edge.type,
                },
                classes: edge.type,
            });
        }

        // Nodes
        for (const node of graphData.nodes) {
            const label = this._buildLabel(node);
            elements.push({
                group: 'nodes',
                data: {
                    id: 'node-' + node.name,
                    nodeName: node.name,
                    displayLabel: label,
                    // tooltip 데이터
                    summary: node.summary,
                    params: node.params,
                    tags: node.tags,
                    isTerminal: !outgoing.has(node.name),
                    isStart: node.name === graphData.startNode,
                    isFunction: node.params && node.params.length > 0,
                },
            });
        }

        return elements;
    }

    /** 노드 라벨 문자열 구성 */
    _buildLabel(node) {
        let label = node.name;

        // 함수면 파라미터 표시
        if (node.params && node.params.length > 0) {
            label += '(' + node.params.join(', ') + ')';
        }

        // 첫 대사 미리보기
        if (node.summary.firstLine) {
            label += '\n' + node.summary.firstLine;
        }

        // 통계 요약
        const stats = [];
        if (node.summary.lineCount > 0) stats.push(node.summary.lineCount + ' lines');
        if (node.summary.choiceCount > 0) stats.push(node.summary.choiceCount + ' choices');
        if (node.summary.hasCondition) stats.push('if');
        if (node.summary.hasRandom) stats.push('random');
        if (stats.length > 0) {
            label += '\n' + stats.join(', ');
        }

        return label;
    }

    /** 노드 클래스 (시작/종단/함수) 적용 */
    _applyNodeClasses(graphData) {
        this.cy.nodes().forEach(node => {
            if (node.data('isStart')) node.addClass('start');
            if (node.data('isTerminal')) node.addClass('terminal');
            if (node.data('isFunction')) node.addClass('function');
        });
    }

    /** 툴팁 DOM 초기화 */
    _initTooltip() {
        this.tooltip = document.createElement('div');
        this.tooltip.className = 'graph-tooltip';
        document.body.appendChild(this.tooltip);
    }

    /** 툴팁 표시 */
    _showTooltip(cyNode, pos) {
        const data = cyNode.data();
        let html = `<div class="tooltip-title">${this._escapeHtml(data.nodeName)}`;
        if (data.params && data.params.length > 0) {
            html += `(${data.params.join(', ')})`;
        }
        html += '</div>';

        const s = data.summary;
        if (s) {
            const parts = [];
            if (s.lineCount > 0) parts.push(`${s.lineCount} dialogue lines`);
            if (s.choiceCount > 0) parts.push(`${s.choiceCount} choices`);
            if (s.hasJump) parts.push('jump');
            if (s.hasCondition) parts.push('condition');
            if (s.hasRandom) parts.push('random');
            if (s.hasCommand) parts.push('commands');
            if (s.characters && s.characters.length > 0) {
                parts.push('chars: ' + s.characters.join(', '));
            }
            if (parts.length > 0) {
                html += `<div class="tooltip-line">${parts.join(' | ')}</div>`;
            }
            if (s.firstLine) {
                html += `<div class="tooltip-line" style="margin-top:4px;color:var(--text);">${this._escapeHtml(s.firstLine)}</div>`;
            }
        }

        if (data.tags && data.tags.length > 0) {
            const tagStr = data.tags.map(t => `#${t.key}${t.value ? '=' + t.value : ''}`).join(' ');
            html += `<div class="tooltip-tags">${this._escapeHtml(tagStr)}</div>`;
        }

        this.tooltip.innerHTML = html;
        this.tooltip.style.display = 'block';

        // 그래프 컨테이너 기준 위치 계산
        const container = document.getElementById(this.containerId);
        const rect = container.getBoundingClientRect();
        this.tooltip.style.left = (rect.left + pos.x + 12) + 'px';
        this.tooltip.style.top = (rect.top + pos.y - 12) + 'px';
    }

    /** 툴팁 숨기기 */
    _hideTooltip() {
        if (this.tooltip) this.tooltip.style.display = 'none';
    }

    /** 뷰포트 중심 좌표 */
    _center() {
        const ext = this.cy.extent();
        return {
            x: this.cy.width() / 2,
            y: this.cy.height() / 2,
        };
    }

    _escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
