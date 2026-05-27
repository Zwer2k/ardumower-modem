  
  

<script lang="ts">
    import type { ConsoleLine } from '../../../model';
    import VirtualList from '../../../widget/VirtualList.svelte';
      import { Toggle } from 'carbon-components-svelte';
    
    let  { 
      consoleLines,
      sendCmd = $bindable(),
      onOutputDone 
    }: { 
      consoleLines: ConsoleLine[],
      sendCmd: string,
      onOutputDone?: () => void
    } = $props();  
  
    let autoscroll = $state(true);
    let bufferLines = 10000;
    
    let command = $state<string>("");
    let output = $state<ConsoleLine[]>([]);
    let history = $state<string[]>([]);
    let historyIndex = $state<number>(-1);
    let commandIndex = 0;
    let scrollToIndex: ((index: any, opts: any) => Promise<void>) | undefined = $state(undefined);

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
          history = [trimmedCommand, ...history];
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

    export function setFous(el: HTMLElement){
        el.focus()
    }
  </script>
  
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
      </div>
    </div>
      </div>
  </main>
  
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
      .autoscroll-toggle {
        padding-top: 0 !important;
      }
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
    .terminal-input-center {
      display: none;
    }
    .autoscroll-label {
      color: #eee;
      font-size: 0.95em;
      user-select: none;
      display: flex;
      align-items: center;
      height: 100%;
    }
    .terminal-input-center {
      display: flex;
      align-items: center;
      justify-content: center;
      flex: 1;
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
    .autoscroll-toggle {
      margin-top: 0 !important;
      align-self: center;
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

/* Carbon Toggle vertical alignment fix */
:global(.bx--toggle__switch) {
  margin-top: 0 !important;
  margin-bottom: 0 !important;
    }
  </style>