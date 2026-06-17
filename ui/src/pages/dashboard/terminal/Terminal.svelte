


<script lang="ts">
    import type { ConsoleLine } from '../../../model';
    import VirtualList from '../../../widget/VirtualList.svelte';
      import { Toggle } from 'carbon-components-svelte';
    import { ubxCommands } from '../gps/ubxCommands';
    
    let  { 
      consoleLines,
      sendCmd = $bindable(),
      onOutputDone 
    }: { 
      consoleLines: ConsoleLine[],
      sendCmd: string,
      onOutputDone?: () => void
    } = $props();  
  
    const HISTORY_KEY = 'ardumower_terminal_history';
    const MAX_HISTORY = 50;
  
    let autoscroll = $state(true);
    let bufferLines = 10000;
    
    let command = $state<string>("");
    let output = $state<ConsoleLine[]>([]);
    let history = $state<string[]>(loadHistory());
    let historyIndex = $state<number>(-1);
    let commandIndex = 0;
    let scrollToIndex: ((index: any, opts: any) => Promise<void>) | undefined = $state(undefined);

    // Known commands context menu
    let showCmdMenu = $state(false);
    let menuPos = $state({ x: 0, y: 0 });
    let inputEl: HTMLInputElement | undefined = $state(undefined);

    const baseCommands: { cmd: string; desc: string }[] = [
      { cmd: 'AT+S', desc: 'Status anfordern (CRC, Position, Lösung)' },
      { cmd: 'AT+T', desc: 'Statistiken anfordern' },
      { cmd: 'AT+V', desc: 'Versionsinfo anfordern' },
      { cmd: 'AT+S3', desc: 'Sensor-Zusammenfassung' },
      { cmd: 'AT+S4', desc: 'GPS-Details / Satelliten' },
      { cmd: 'AT+Q', desc: 'Motor-Plot Daten' },
      { cmd: 'AT+M,0,0', desc: 'Manuell fahren (linear, angular)' },
      { cmd: 'AT+W,50', desc: 'Mähanteil in % setzen' },
      { cmd: 'AT+S2,30', desc: 'Schnitthöhe setzen (mm)' },
    ];

    const gpsDashboardEnabled = import.meta.env.VITE_ENABLE_GPS_DASHBOARD === 'true';
    const ubxCommandItems: { cmd: string; desc: string }[] = gpsDashboardEnabled
      ? ubxCommands
          .filter(c => c.hex)
          .map(c => ({ cmd: `AT+U,${c.hex}`, desc: `${c.name} (${c.category})` }))
      : [];

    const knownCommands: { cmd: string; desc: string }[] = [...baseCommands, ...ubxCommandItems];

    function loadHistory(): string[] {
      if (typeof document === 'undefined') return [];
      try {
        const raw = document.cookie.split('; ').find(row => row.startsWith(HISTORY_KEY + '='));
        if (!raw) return [];
        const decoded = decodeURIComponent(raw.substring(HISTORY_KEY.length + 1));
        const parsed = JSON.parse(decoded);
        return Array.isArray(parsed) ? parsed.slice(0, MAX_HISTORY) : [];
      } catch (e) {
        console.warn('[Terminal] could not load history from cookie', e);
        return [];
      }
    }

    function saveHistory(items: string[]) {
      if (typeof document === 'undefined') return;
      try {
        const value = encodeURIComponent(JSON.stringify(items.slice(0, MAX_HISTORY)));
        // 365 days, SameSite lax
        document.cookie = `${HISTORY_KEY}=${value}; path=/; max-age=${60 * 60 * 24 * 365}; SameSite=Lax`;
      } catch (e) {
        console.warn('[Terminal] could not save history to cookie', e);
      }
    }

    function addToHistory(cmd: string) {
      const trimmed = cmd.trim();
      if (!trimmed) return;
      // remove duplicate at front
      history = [trimmed, ...history.filter(h => h !== trimmed)].slice(0, MAX_HISTORY);
      saveHistory(history);
    }

    $effect(() => {
      if (consoleLines != null && consoleLines.length > 0) {
        output = [...output, ...consoleLines];
        let remove = output.length - bufferLines;
        if (remove > 0) {
          output.splice(0, remove);
        }

        if (autoscroll && scrollToIndex != undefined) {
          scrollToIndex(output.length - 1, { behavior: 'smooth' });
        }

        if (onOutputDone) {
          onOutputDone();
        }
      }
    });

  
    function processCommand(event: KeyboardEvent): void {
      if (event.key === 'Enter') {
        const trimmedCommand: string = command.trim();
        if (trimmedCommand) {
          addToHistory(trimmedCommand);
          historyIndex = -1;
  
          executeCommand(trimmedCommand);
  
          command = '';
        }
      } else if (event.key === 'ArrowUp') {
        if (history.length > 0 && historyIndex < history.length - 1) {
          historyIndex++;
          command = history[historyIndex];
          event.preventDefault();
        }
      } else if (event.key === 'ArrowDown') {
        if (historyIndex > 0) {
          historyIndex--;
          command = history[historyIndex];
          event.preventDefault();
        } else if (historyIndex === 0) {
          historyIndex = -1;
          command = '';
          event.preventDefault();
        }
      }
    }
  
    function executeCommand(cmd: string): void {
      switch (cmd.toLowerCase()) {
        case 'clear':
          output = [];
          return;
        default:
          sendCmd = '';
          sendCmd = cmd;
          break;
      }
    }

    function onContextMenu(event: MouseEvent) {
      event.preventDefault();
      menuPos = { x: event.clientX, y: event.clientY };
      showCmdMenu = true;
    }

    function selectKnownCommand(cmd: string) {
      command = cmd;
      showCmdMenu = false;
      inputEl?.focus();
    }

    function closeMenu() {
      showCmdMenu = false;
    }

    function downloadLog(): void {
      const lines = output.map(l => l.text).join('\n');
      const blob = new Blob([lines], { type: 'text/plain;charset=utf-8' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `ardumower-log-${new Date().toISOString().replace(/[:.]/g, '-')}.txt`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }

    export function setFous(el: HTMLElement){
        el.focus()
    }
  </script>
  
  <svelte:window onclick={closeMenu} />

  <main class="terminal-container">
      <div class="terminal-wrapper" style="height:100%;display:flex;flex-direction:column;">
    <div class="terminal-output-virtual">
        <div class="list-holder">
          <VirtualList items={output}
              height="calc(100% - 20px)"
              bind:scrollToIndex
              let:item>
              <div class="console-line">
                  <div class="text">{item.text}</div>
              </div>
          </VirtualList>
        </div>
    </div>
    <div class="terminal-input-wrapper">
      <span class="prompt">$&nbsp;</span>
      <input
        type="text"
        class="terminal-input"
        bind:value={command}
        onkeydown={processCommand}
        oncontextmenu={onContextMenu}
        bind:this={inputEl}
        spellcheck="false"
        use:setFous
      />
      <div class="terminal-input-actions">
        <span class="autoscroll-label">Autoscroll</span>
        <Toggle
          class="autoscroll-toggle"
          labelText=""
          labelA={""}
          labelB={""}
          bind:toggled={autoscroll}
        />
        <button class="download-btn" onclick={downloadLog} title="Download log as text file">
          Download Log
        </button>
      </div>
    </div>
      </div>
  </main>

  {#if showCmdMenu}
  <div class="cmd-menu" style="left: {menuPos.x}px; top: {menuPos.y}px;" role="menu">
    <div class="cmd-menu-title">Bekannte Befehle</div>
    {#each knownCommands as item}
      <button class="cmd-menu-item" onclick={() => selectKnownCommand(item.cmd)} title={item.desc}>
        <span class="cmd-menu-cmd">{item.cmd}</span>
        <span class="cmd-menu-desc">{item.desc}</span>
      </button>
    {/each}
  </div>
  {/if}
  
  <style>
  
    .terminal-container {
      height: calc(100vh - 48px); /* Vollhöhe minus Header */
      background-color: #1a1a1a;
      border-radius: 2px;
      box-shadow: 0 4px 10px rgba(0, 0, 0, 0.5);
      display: flex;
      flex-direction: column;
      overflow: hidden;
    }
    
    .terminal-output-virtual {
      flex-grow: 1;
      padding: 10px 15px;
      background-color: #1a1a1a;
      color: #eee;
      white-space: pre-wrap;
      position: relative;

      .list-holder {
        height: 100%;
        position: absolute;
        width: calc(100% - 20px);
      }
    }
    
    .terminal-input-wrapper {
      display: flex;
      align-items: center;
      padding: 10px 15px;
      border-top: 1px solid #333;
      background-color: #1a1a1a;
    }
    .autoscroll-label {
      color: #eee;
      font-size: 0.95em;
      user-select: none;
      display: flex;
      align-items: center;
      height: 100%;
    }
    .terminal-input-actions {
      display: flex;
      align-items: center;
      margin-left: auto;
      gap: 8px;
      margin-top: 0 !important;
      height: 40px;
      min-height: 40px;
    }
    .autoscroll-label {
      color: #eee;
      font-size: 0.95em;
      user-select: none;
      align-self: center;
      line-height: 1;
      height: 32px;
      display: flex;
      align-items: center;
    }
  
    .prompt {
      color: #00ff00;
      margin-right: 5px;
    }
  
    .terminal-input {
      flex-grow: 1;
      background: transparent;
      border: none;
      outline: none;
      color: #eee;
      font-family: inherit;
      font-size: inherit;
      caret-color: #00ff00;
    }

    .console-line {
        display: flex;
        padding: 3px; 
    }

    .console-line .text {
  line-break: anywhere;
}

.download-btn {
  background: #2a2a2a;
  color: #ccc;
  border: 1px solid #444;
  border-radius: 4px;
  padding: 4px 10px;
  font-size: 0.85em;
  cursor: pointer;
  line-height: 1.4;
  white-space: nowrap;
}
.download-btn:hover {
  background: #3a3a3a;
  color: #fff;
  border-color: #666;
}

.cmd-menu {
  position: fixed;
  z-index: 1000;
  min-width: 260px;
  max-width: 400px;
  max-height: 70vh;
  overflow-y: auto;
  background: #252525;
  border: 1px solid #444;
  border-radius: 4px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.6);
  padding: 6px 0;
  font-size: 0.85em;
}

.cmd-menu-title {
  padding: 6px 12px;
  color: #888;
  border-bottom: 1px solid #383838;
  margin-bottom: 4px;
  font-weight: 600;
}

.cmd-menu-item {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  width: 100%;
  padding: 6px 12px;
  background: transparent;
  border: none;
  color: #eee;
  cursor: pointer;
  text-align: left;
}

.cmd-menu-item:hover {
  background: #333;
}

.cmd-menu-cmd {
  font-family: monospace;
  color: #00ff00;
  font-size: 0.95em;
}

.cmd-menu-desc {
  color: #aaa;
  font-size: 0.85em;
  margin-top: 2px;
}

/* Carbon Toggle vertical alignment fix */
:global(.bx--toggle__switch) {
  margin-top: 0 !important;
  margin-bottom: 0 !important;
    }
  </style>
