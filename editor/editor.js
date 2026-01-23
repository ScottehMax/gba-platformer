// GBA Level Editor - Main JavaScript
// Handles level editing, tile placement, object management, and file I/O

class LevelEditor {
    constructor() {
        this.canvas = document.getElementById('editorCanvas');
        this.ctx = this.canvas.getContext('2d');
        
        // Level data
        this.level = {
            name: 'Untitled Level',
            author: '',
            width: 60,
            height: 20,
            tileWidth: 8,
            tileHeight: 8,
            tiles: [],
            objects: [],
            playerSpawn: { x: 40, y: 96 }
        };
        
        // Editor state
        this.zoom = 2.0;
        this.camera = { x: 0, y: 0 };
        this.currentTool = 'brush';
        this.selectedTile = 0;
        this.selectedObjectType = 'player_spawn';
        this.showGrid = true;
        this.showObjects = true;
        
        // Interaction state
        this.isDragging = false;
        this.isPanning = false;
        this.lastMousePos = { x: 0, y: 0 };
        
        // Undo/redo stacks
        this.undoStack = [];
        this.redoStack = [];
        this.maxUndoSteps = 50;
        
        // Tile images
        this.tileImages = [];
        this.tilesLoaded = false;
        this.tilesetColumns = 4;  // Default columns for tile palette grid
        this.tilesetRows = 1;
        this.tilesetImage = null;  // Full tileset image for palette display
        
        // Initialize
        this.initLevel();
        this.loadTileImages();
        this.setupEventListeners();
        this.resize();
        this.render();
    }
    
    initLevel() {
        // Initialize empty tile grid
        this.level.tiles = [];
        for (let y = 0; y < this.level.height; y++) {
            const row = [];
            for (let x = 0; x < this.level.width; x++) {
                row.push(0);
            }
            this.level.tiles.push(row);
        }
        
        // Clear undo/redo
        this.undoStack = [];
        this.redoStack = [];
        this.updateUndoRedoButtons();
    }
    
    loadTileImages() {
        // Load tile assets from PNG files
        this.tileImages = [];

        // Create tile 0 (sky/air) as placeholder - dark blue
        const skyTile = document.createElement('canvas');
        skyTile.width = 8;
        skyTile.height = 8;
        const skyCtx = skyTile.getContext('2d');
        skyCtx.fillStyle = '#183478'; // Dark blue to match game
        skyCtx.fillRect(0, 0, 8, 8);
        this.tileImages[0] = skyTile;

        // Load Grassy_stone.png (tiles 1+)
        const grassyStoneImg = new Image();
        grassyStoneImg.onload = () => {
            // Calculate number of tiles based on image dimensions (8x8 tiles)
            const tilesWide = Math.floor(grassyStoneImg.width / 8);
            const tilesTall = Math.floor(grassyStoneImg.height / 8);
            const numTiles = tilesWide * tilesTall;
            this.tilesetColumns = tilesWide;
            this.tilesetRows = tilesTall;
            this.tilesetImage = grassyStoneImg;
            this.extractTiles(grassyStoneImg, 1, numTiles);
            this.tilesLoaded = true;
            this.renderTilePalette();
        };
        grassyStoneImg.onerror = () => {
            console.error('Failed to load Grassy_stone.png');
            this.tilesetColumns = 11;
            this.tilesetRows = 5;
            this.createPlaceholderTiles(1, 55, '#8B4513');
            this.tilesLoaded = true;
            this.renderTilePalette();
        };
        grassyStoneImg.src = 'Grassy_stone.png';
    }

    extractTiles(image, startIndex, count) {
        // Extract 8x8 tiles from the image
        // Tiles are arranged in a grid
        const tilesPerRow = Math.floor(image.width / 8);
        const tilesPerCol = Math.floor(image.height / 8);
        const totalTiles = count !== undefined ? count : tilesPerRow * tilesPerCol;

        for (let i = 0; i < totalTiles; i++) {
            const tileX = (i % tilesPerRow) * 8;
            const tileY = Math.floor(i / tilesPerRow) * 8;

            const canvas = document.createElement('canvas');
            canvas.width = 8;
            canvas.height = 8;
            const ctx = canvas.getContext('2d');
            ctx.drawImage(image, tileX, tileY, 8, 8, 0, 0, 8, 8);

            this.tileImages[startIndex + i] = canvas;
        }
    }

    createPlaceholderTiles(startIndex, count, baseColor) {
        // Fallback if image loading fails
        for (let i = 0; i < count; i++) {
            const canvas = document.createElement('canvas');
            canvas.width = 8;
            canvas.height = 8;
            const ctx = canvas.getContext('2d');
            ctx.fillStyle = baseColor;
            ctx.fillRect(0, 0, 8, 8);
            this.tileImages[startIndex + i] = canvas;
        }
    }
    
    renderTilePalette() {
        const palette = document.getElementById('tilePalette');
        palette.innerHTML = '';

        const scale = 4; // Scale up for visibility

        // Create canvas for tile 0 (sky)
        const skyCanvas = document.createElement('canvas');
        skyCanvas.width = 8 * scale;
        skyCanvas.height = 8 * scale;
        skyCanvas.style.cursor = 'pointer';
        skyCanvas.style.imageRendering = 'pixelated';
        skyCanvas.style.imageRendering = 'crisp-edges';
        skyCanvas.style.marginBottom = '8px';
        skyCanvas.style.border = this.selectedTile === 0 ? '2px solid #61dafb' : '1px solid #404040';

        const skyCtx = skyCanvas.getContext('2d');
        skyCtx.imageSmoothingEnabled = false;
        if (this.tileImages[0]) {
            skyCtx.drawImage(this.tileImages[0], 0, 0, 8 * scale, 8 * scale);
        }

        skyCanvas.addEventListener('click', () => {
            this.selectedTile = 0;
            this.renderTilePalette();
            if (this.currentTool === 'eraser') {
                this.setTool('brush');
            }
        });

        // Create label for sky tile
        const skyLabel = document.createElement('div');
        skyLabel.textContent = 'Tile 0 (Sky)';
        skyLabel.style.fontSize = '12px';
        skyLabel.style.marginBottom = '12px';
        skyLabel.style.color = '#b0b0b0';

        // Create canvas for tileset palette
        const canvas = document.createElement('canvas');
        canvas.width = this.tilesetColumns * 8 * scale;
        canvas.height = this.tilesetRows * 8 * scale;
        canvas.style.cursor = 'pointer';
        canvas.style.imageRendering = 'pixelated';
        canvas.style.imageRendering = 'crisp-edges';

        const ctx = canvas.getContext('2d');
        ctx.imageSmoothingEnabled = false;

        // Draw the full tileset image
        if (this.tilesetImage) {
            ctx.drawImage(this.tilesetImage, 0, 0, canvas.width, canvas.height);
        }

        // Draw grid
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.3)';
        ctx.lineWidth = 1;
        for (let x = 0; x <= this.tilesetColumns; x++) {
            ctx.beginPath();
            ctx.moveTo(x * 8 * scale, 0);
            ctx.lineTo(x * 8 * scale, canvas.height);
            ctx.stroke();
        }
        for (let y = 0; y <= this.tilesetRows; y++) {
            ctx.beginPath();
            ctx.moveTo(0, y * 8 * scale);
            ctx.lineTo(canvas.width, y * 8 * scale);
            ctx.stroke();
        }

        // Highlight selected tile (if not tile 0)
        if (this.selectedTile > 0 && this.selectedTile < this.tileImages.length) {
            // Adjust for tile 0 offset
            const adjustedIndex = this.selectedTile - 1;
            const tileX = adjustedIndex % this.tilesetColumns;
            const tileY = Math.floor(adjustedIndex / this.tilesetColumns);

            ctx.strokeStyle = '#61dafb';
            ctx.lineWidth = 2;
            ctx.strokeRect(tileX * 8 * scale + 1, tileY * 8 * scale + 1, 8 * scale - 2, 8 * scale - 2);
        }

        // Handle clicks
        canvas.addEventListener('click', (e) => {
            const rect = canvas.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const y = e.clientY - rect.top;

            const tileX = Math.floor(x / (8 * scale));
            const tileY = Math.floor(y / (8 * scale));
            const adjustedIndex = tileY * this.tilesetColumns + tileX;
            const tileIndex = adjustedIndex + 1; // Add 1 to account for tile 0

            if (tileIndex > 0 && tileIndex < this.tileImages.length) {
                this.selectedTile = tileIndex;
                this.renderTilePalette();
                if (this.currentTool === 'eraser') {
                    this.setTool('brush');
                }
            }
        });

        palette.appendChild(skyCanvas);
        palette.appendChild(skyLabel);
        palette.appendChild(canvas);
    }
    
    setupEventListeners() {
        // Canvas events
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        this.canvas.addEventListener('contextmenu', (e) => e.preventDefault());
        this.canvas.addEventListener('wheel', (e) => this.onWheel(e));
        
        // Window events
        window.addEventListener('resize', () => this.resize());
        window.addEventListener('keydown', (e) => this.onKeyDown(e));
        
        // UI events
        document.getElementById('newLevel').addEventListener('click', () => this.newLevel());
        document.getElementById('loadLevel').addEventListener('click', () => this.loadLevel());
        document.getElementById('saveLevel').addEventListener('click', () => this.saveLevel());
        document.getElementById('fileInput').addEventListener('change', (e) => this.handleFileLoad(e));
        document.getElementById('undo').addEventListener('click', () => this.undo());
        document.getElementById('redo').addEventListener('click', () => this.redo());
        document.getElementById('showGrid').addEventListener('change', (e) => {
            this.showGrid = e.target.checked;
            this.render();
        });
        document.getElementById('showObjects').addEventListener('change', (e) => {
            this.showObjects = e.target.checked;
            this.render();
        });
        
        // Tool buttons
        document.querySelectorAll('.tool-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                this.setTool(btn.dataset.tool);
            });
        });
        
        // Level properties
        document.getElementById('levelName').addEventListener('change', (e) => {
            this.level.name = e.target.value;
        });
        document.getElementById('levelAuthor').addEventListener('change', (e) => {
            this.level.author = e.target.value;
        });
        document.getElementById('resizeLevel').addEventListener('click', () => this.resizeLevel());
        
        // Object tools
        document.getElementById('objectType').addEventListener('change', (e) => {
            this.selectedObjectType = e.target.value;
        });
        document.getElementById('setPlayerSpawn').addEventListener('click', () => this.setPlayerSpawnAtMouse());
        
        // Zoom controls
        document.getElementById('zoomIn').addEventListener('click', () => this.changeZoom(0.5));
        document.getElementById('zoomOut').addEventListener('click', () => this.changeZoom(-0.5));
        document.getElementById('resetView').addEventListener('click', () => this.resetView());
        
        // Update UI
        this.updateSpawnInfo();
    }
    
    setTool(tool) {
        this.currentTool = tool;
        document.querySelectorAll('.tool-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tool === tool);
        });
        document.getElementById('currentTool').textContent = 
            tool.charAt(0).toUpperCase() + tool.slice(1);
        
        // Update cursor
        const wrapper = document.querySelector('.canvas-wrapper');
        if (tool === 'object') {
            wrapper.style.cursor = 'pointer';
        } else {
            wrapper.style.cursor = 'crosshair';
        }
    }
    
    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        this.lastMousePos = { x, y };
        
        // Middle mouse or space + left mouse for panning
        if (e.button === 1 || (e.button === 0 && e.shiftKey)) {
            this.isPanning = true;
            e.preventDefault();
            return;
        }
        
        // Right click - eraser
        if (e.button === 2) {
            const worldPos = this.screenToWorld(x, y);
            const tileX = Math.floor(worldPos.x / 8);
            const tileY = Math.floor(worldPos.y / 8);
            this.placeTile(tileX, tileY, 0);
            this.isDragging = true;
            return;
        }
        
        // Left click - use current tool
        if (e.button === 0) {
            this.useTool(x, y);
            this.isDragging = true;
        }
    }
    
    onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Update mouse position display
        const worldPos = this.screenToWorld(x, y);
        const tileX = Math.floor(worldPos.x / 8);
        const tileY = Math.floor(worldPos.y / 8);
        document.getElementById('mousePos').textContent = 
            `Tile: (${tileX}, ${tileY}) Px: (${Math.floor(worldPos.x)}, ${Math.floor(worldPos.y)})`;
        
        // Panning
        if (this.isPanning) {
            const dx = x - this.lastMousePos.x;
            const dy = y - this.lastMousePos.y;
            this.camera.x -= dx / this.zoom;
            this.camera.y -= dy / this.zoom;
            this.render();
        }
        
        // Drawing
        if (this.isDragging && (this.currentTool === 'brush' || this.currentTool === 'eraser' || e.buttons === 2)) {
            const tileId = e.buttons === 2 ? 0 : this.selectedTile;
            this.placeTile(tileX, tileY, tileId);
        }
        
        this.lastMousePos = { x, y };
    }
    
    onMouseUp(e) {
        this.isDragging = false;
        this.isPanning = false;
    }
    
    onWheel(e) {
        e.preventDefault();
        const delta = e.deltaY > 0 ? -0.2 : 0.2;
        this.changeZoom(delta);
    }
    
    onKeyDown(e) {
        // Tool shortcuts
        if (e.key === 'b' || e.key === 'B') this.setTool('brush');
        if (e.key === 'e' || e.key === 'E') this.setTool('eraser');
        if (e.key === 'f' || e.key === 'F') this.setTool('fill');
        if (e.key === 'o' || e.key === 'O') this.setTool('object');
        
        // Grid toggle
        if (e.key === 'g' || e.key === 'G') {
            const checkbox = document.getElementById('showGrid');
            checkbox.checked = !checkbox.checked;
            this.showGrid = checkbox.checked;
            this.render();
        }
        
        // Undo/Redo
        if (e.ctrlKey || e.metaKey) {
            if (e.key === 'z' || e.key === 'Z') {
                e.preventDefault();
                if (e.shiftKey) {
                    this.redo();
                } else {
                    this.undo();
                }
            }
            if (e.key === 'y' || e.key === 'Y') {
                e.preventDefault();
                this.redo();
            }
        }
    }
    
    useTool(screenX, screenY) {
        const worldPos = this.screenToWorld(screenX, screenY);
        const tileX = Math.floor(worldPos.x / 8);
        const tileY = Math.floor(worldPos.y / 8);
        
        switch (this.currentTool) {
            case 'brush':
                this.placeTile(tileX, tileY, this.selectedTile);
                break;
            case 'eraser':
                this.placeTile(tileX, tileY, 0);
                break;
            case 'fill':
                this.floodFill(tileX, tileY, this.selectedTile);
                break;
            case 'object':
                this.placeObject(worldPos.x, worldPos.y);
                break;
        }
    }
    
    placeTile(tileX, tileY, tileId) {
        if (tileX < 0 || tileX >= this.level.width || tileY < 0 || tileY >= this.level.height) {
            return;
        }
        
        if (this.level.tiles[tileY][tileX] !== tileId) {
            this.pushUndo();
            this.level.tiles[tileY][tileX] = tileId;
            this.render();
        }
    }
    
    floodFill(startX, startY, newTileId) {
        if (startX < 0 || startX >= this.level.width || startY < 0 || startY >= this.level.height) {
            return;
        }
        
        const oldTileId = this.level.tiles[startY][startX];
        if (oldTileId === newTileId) return;
        
        this.pushUndo();
        
        const stack = [[startX, startY]];
        const visited = new Set();
        
        while (stack.length > 0) {
            const [x, y] = stack.pop();
            const key = `${x},${y}`;
            
            if (visited.has(key)) continue;
            if (x < 0 || x >= this.level.width || y < 0 || y >= this.level.height) continue;
            if (this.level.tiles[y][x] !== oldTileId) continue;
            
            visited.add(key);
            this.level.tiles[y][x] = newTileId;
            
            stack.push([x + 1, y]);
            stack.push([x - 1, y]);
            stack.push([x, y + 1]);
            stack.push([x, y - 1]);
        }
        
        this.render();
    }
    
    placeObject(x, y) {
        if (this.selectedObjectType === 'player_spawn') {
            this.level.playerSpawn = { x: Math.floor(x), y: Math.floor(y) };
            this.updateSpawnInfo();
            this.render();
        } else {
            this.pushUndo();
            this.level.objects.push({
                type: this.selectedObjectType,
                x: Math.floor(x),
                y: Math.floor(y),
                properties: {}
            });
            this.renderObjectList();
            this.render();
        }
    }
    
    screenToWorld(screenX, screenY) {
        return {
            x: screenX / this.zoom + this.camera.x,
            y: screenY / this.zoom + this.camera.y
        };
    }
    
    worldToScreen(worldX, worldY) {
        return {
            x: (worldX - this.camera.x) * this.zoom,
            y: (worldY - this.camera.y) * this.zoom
        };
    }
    
    changeZoom(delta) {
        const oldZoom = this.zoom;
        this.zoom = Math.max(0.5, Math.min(8, this.zoom + delta));
        
        // Adjust camera to zoom toward center
        const centerX = this.canvas.width / 2;
        const centerY = this.canvas.height / 2;
        const worldX = centerX / oldZoom + this.camera.x;
        const worldY = centerY / oldZoom + this.camera.y;
        this.camera.x = worldX - centerX / this.zoom;
        this.camera.y = worldY - centerY / this.zoom;
        
        document.getElementById('zoomLevel').textContent = Math.round(this.zoom * 100) + '%';
        this.render();
    }
    
    resetView() {
        this.zoom = 2.0;
        this.camera = { x: 0, y: 0 };
        document.getElementById('zoomLevel').textContent = '200%';
        this.render();
    }
    
    resize() {
        const wrapper = this.canvas.parentElement;
        this.canvas.width = wrapper.clientWidth;
        this.canvas.height = wrapper.clientHeight;
        this.render();
    }
    
    render() {
        if (!this.tilesLoaded) return;
        
        const ctx = this.ctx;
        const canvasWidth = this.canvas.width;
        const canvasHeight = this.canvas.height;
        
        // Clear
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, canvasWidth, canvasHeight);
        
        // Calculate visible tile range
        const startTileX = Math.max(0, Math.floor(this.camera.x / 8));
        const startTileY = Math.max(0, Math.floor(this.camera.y / 8));
        const endTileX = Math.min(this.level.width, Math.ceil((this.camera.x + canvasWidth / this.zoom) / 8));
        const endTileY = Math.min(this.level.height, Math.ceil((this.camera.y + canvasHeight / this.zoom) / 8));
        
        ctx.save();
        ctx.scale(this.zoom, this.zoom);
        ctx.translate(-this.camera.x, -this.camera.y);
        
        // Draw tiles
        ctx.imageSmoothingEnabled = false;
        for (let y = startTileY; y < endTileY; y++) {
            for (let x = startTileX; x < endTileX; x++) {
                const tileId = this.level.tiles[y][x];
                if (tileId >= 0 && tileId < this.tileImages.length) {
                    ctx.drawImage(this.tileImages[tileId], x * 8, y * 8);
                }
            }
        }
        
        // Draw grid
        if (this.showGrid) {
            ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
            ctx.lineWidth = 1 / this.zoom;
            for (let x = startTileX; x <= endTileX; x++) {
                ctx.beginPath();
                ctx.moveTo(x * 8, startTileY * 8);
                ctx.lineTo(x * 8, endTileY * 8);
                ctx.stroke();
            }
            for (let y = startTileY; y <= endTileY; y++) {
                ctx.beginPath();
                ctx.moveTo(startTileX * 8, y * 8);
                ctx.lineTo(endTileX * 8, y * 8);
                ctx.stroke();
            }
        }
        
        // Draw objects
        if (this.showObjects) {
            // Player spawn
            ctx.fillStyle = 'rgba(0, 255, 0, 0.5)';
            ctx.fillRect(this.level.playerSpawn.x - 8, this.level.playerSpawn.y - 8, 16, 16);
            ctx.strokeStyle = '#0f0';
            ctx.lineWidth = 2 / this.zoom;
            ctx.strokeRect(this.level.playerSpawn.x - 8, this.level.playerSpawn.y - 8, 16, 16);
            
            // Other objects
            this.level.objects.forEach(obj => {
                ctx.fillStyle = 'rgba(255, 0, 0, 0.5)';
                ctx.fillRect(obj.x - 4, obj.y - 4, 8, 8);
                ctx.strokeStyle = '#f00';
                ctx.lineWidth = 1 / this.zoom;
                ctx.strokeRect(obj.x - 4, obj.y - 4, 8, 8);
            });
        }
        
        ctx.restore();
    }
    
    pushUndo() {
        // Deep copy current state
        const state = JSON.parse(JSON.stringify({
            tiles: this.level.tiles,
            objects: this.level.objects,
            playerSpawn: this.level.playerSpawn
        }));
        
        this.undoStack.push(state);
        if (this.undoStack.length > this.maxUndoSteps) {
            this.undoStack.shift();
        }
        
        this.redoStack = [];
        this.updateUndoRedoButtons();
    }
    
    undo() {
        if (this.undoStack.length === 0) return;
        
        // Save current state to redo
        const currentState = JSON.parse(JSON.stringify({
            tiles: this.level.tiles,
            objects: this.level.objects,
            playerSpawn: this.level.playerSpawn
        }));
        this.redoStack.push(currentState);
        
        // Restore previous state
        const prevState = this.undoStack.pop();
        this.level.tiles = prevState.tiles;
        this.level.objects = prevState.objects;
        this.level.playerSpawn = prevState.playerSpawn;
        
        this.updateUndoRedoButtons();
        this.updateSpawnInfo();
        this.renderObjectList();
        this.render();
    }
    
    redo() {
        if (this.redoStack.length === 0) return;
        
        // Save current state to undo
        const currentState = JSON.parse(JSON.stringify({
            tiles: this.level.tiles,
            objects: this.level.objects,
            playerSpawn: this.level.playerSpawn
        }));
        this.undoStack.push(currentState);
        
        // Restore next state
        const nextState = this.redoStack.pop();
        this.level.tiles = nextState.tiles;
        this.level.objects = nextState.objects;
        this.level.playerSpawn = nextState.playerSpawn;
        
        this.updateUndoRedoButtons();
        this.updateSpawnInfo();
        this.renderObjectList();
        this.render();
    }
    
    updateUndoRedoButtons() {
        document.getElementById('undo').disabled = this.undoStack.length === 0;
        document.getElementById('redo').disabled = this.redoStack.length === 0;
    }
    
    newLevel() {
        if (confirm('Create a new level? Unsaved changes will be lost.')) {
            this.level = {
                name: 'Untitled Level',
                author: '',
                width: 60,
                height: 20,
                tileWidth: 8,
                tileHeight: 8,
                tiles: [],
                objects: [],
                playerSpawn: { x: 40, y: 96 }
            };
            this.initLevel();
            document.getElementById('levelName').value = this.level.name;
            document.getElementById('levelAuthor').value = this.level.author;
            document.getElementById('levelWidth').value = this.level.width;
            document.getElementById('levelHeight').value = this.level.height;
            this.updateSpawnInfo();
            this.renderObjectList();
            this.resetView();
        }
    }
    
    loadLevel() {
        document.getElementById('fileInput').click();
    }
    
    handleFileLoad(e) {
        const file = e.target.files[0];
        if (!file) return;
        
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const data = JSON.parse(e.target.result);
                this.level = data;
                
                // Update UI
                document.getElementById('levelName').value = this.level.name;
                document.getElementById('levelAuthor').value = this.level.author || '';
                document.getElementById('levelWidth').value = this.level.width;
                document.getElementById('levelHeight').value = this.level.height;
                
                this.undoStack = [];
                this.redoStack = [];
                this.updateUndoRedoButtons();
                this.updateSpawnInfo();
                this.renderObjectList();
                this.resetView();
                
                alert('Level loaded successfully!');
            } catch (err) {
                alert('Error loading level: ' + err.message);
            }
        };
        reader.readAsText(file);
        
        // Reset file input
        e.target.value = '';
    }
    
    saveLevel() {
        // Prepare level data
        const levelData = {
            name: this.level.name,
            author: this.level.author,
            width: this.level.width,
            height: this.level.height,
            tileWidth: this.level.tileWidth,
            tileHeight: this.level.tileHeight,
            tiles: this.level.tiles,
            objects: this.level.objects,
            playerSpawn: this.level.playerSpawn
        };
        
        // Create JSON file
        const json = JSON.stringify(levelData, null, 2);
        const blob = new Blob([json], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        
        // Download
        const a = document.createElement('a');
        a.href = url;
        a.download = this.level.name.replace(/[^a-z0-9]/gi, '_').toLowerCase() + '.json';
        a.click();
        
        URL.revokeObjectURL(url);
    }
    
    resizeLevel() {
        const newWidth = parseInt(document.getElementById('levelWidth').value);
        const newHeight = parseInt(document.getElementById('levelHeight').value);
        
        if (newWidth < 10 || newWidth > 256 || newHeight < 10 || newHeight > 256) {
            alert('Dimensions must be between 10 and 256');
            return;
        }
        
        if (!confirm('Resize level? This will clear all tiles.')) {
            return;
        }
        
        this.pushUndo();
        this.level.width = newWidth;
        this.level.height = newHeight;
        this.initLevel();
        this.render();
    }
    
    setPlayerSpawnAtMouse() {
        const centerX = this.canvas.width / 2;
        const centerY = this.canvas.height / 2;
        const worldPos = this.screenToWorld(centerX, centerY);
        this.level.playerSpawn = { x: Math.floor(worldPos.x), y: Math.floor(worldPos.y) };
        this.updateSpawnInfo();
        this.render();
    }
    
    updateSpawnInfo() {
        document.getElementById('spawnInfo').textContent = 
            `(${this.level.playerSpawn.x}, ${this.level.playerSpawn.y})`;
    }
    
    renderObjectList() {
        const list = document.getElementById('objectList');
        
        if (this.level.objects.length === 0) {
            list.innerHTML = '<p class="empty-message">No objects placed</p>';
            return;
        }
        
        list.innerHTML = '';
        this.level.objects.forEach((obj, index) => {
            const item = document.createElement('div');
            item.className = 'object-item';
            
            const info = document.createElement('span');
            info.textContent = `${obj.type} (${obj.x}, ${obj.y})`;
            
            const deleteBtn = document.createElement('button');
            deleteBtn.textContent = 'Delete';
            deleteBtn.addEventListener('click', () => {
                this.pushUndo();
                this.level.objects.splice(index, 1);
                this.renderObjectList();
                this.render();
            });
            
            item.appendChild(info);
            item.appendChild(deleteBtn);
            list.appendChild(item);
        });
    }
}

// Initialize editor when page loads
window.addEventListener('DOMContentLoaded', () => {
    new LevelEditor();
});
