<script lang="ts">
    import { Dropdown, Toggle } from "carbon-components-svelte";
    import VirtualList from "../../widget/VirtualList.svelte";
    import type { LogLevelDescT, LogLine } from "../../model";
    import type { DropdownItem } from "carbon-components-svelte/src/Dropdown/Dropdown.svelte";

    export let logLevels: LogLevelDescT;
    export let dbgLevels: DropdownItem[];
    export let logData: LogLine[];
    export let dbgLevel: number; 
    
    let items: LogLine[] = [];

    let autoscroll = true;

	let scrollToIndex: ((index: any, opts: any) => Promise<void>) | undefined = undefined;
    let logLevelIndex: number | undefined;
    let logLines = 10000;
    let lineCounter = 0;

    $: if (logData) {
        if (logData != null) {
            items = [...items, ...logData];
            logData = [];
            let remove = items.length - logLines;
            if (remove > 0)
                items.splice(0, remove); 
        
            if (autoscroll && scrollToIndex != undefined) {
                scrollToIndex(items.length - 1, { behavior: 'smooth' });
            }
        }
    }

    // function onScroll() {
    //     autoscroll = list.getOffset() > list.getScrollSize() - list.getClientSize() - 10;
    // }

    logLevelIndex = dbgLevels.findIndex(item => item.id == dbgLevel);
    if (logLevelIndex == -1)  {
        logLevelIndex = 3;
    }

    function onBottom() {
        console.log("bottom");
    }

    function checkLineNr(lineNr: number) {
        if (lineCounter != lineNr) {
            lineCounter = lineNr+1;
            return true;
        }
        lineCounter++;
        return false;
    }

    function getLineCounter(lineNr: number) {
        console.log(lineNr, lineCounter)
        return lineNr;
    }

    $: { dbgLevel = dbgLevels[logLevelIndex].id; }

</script>

<div class="modem-log">
    <div class="log-list">
        <div class="settings">
            <Dropdown
                bind:selectedId={dbgLevel}
                type="inline"
                titleText="Log level"
                items={dbgLevels}/>
            <Toggle
                class="autoscroll-toggle"
                labelText="Autoscroll" 
                labelA={""}
                labelB={""}
                bind:toggled={autoscroll}/>
        </div>
        <VirtualList {items}
            height="calc(100% - 40px)"
            bind:scrollToIndex
            let:item>
            <div class="log-line {checkLineNr(item.nr) ? 'ignore' : ''}">
                <div class="nr">{item.nr}:</div>
                <div class="level level-{logLevels[(item as LogLine).level]}">{logLevels[(item as LogLine).level]}:</div>
                <div class="free-heap">{(item as LogLine).freeHeap}:</div>
                <div class="text">{item.text}</div>
            </div>
        </VirtualList>
    </div>
</div>

<style lang="scss">
    .modem-log {
        :global(.bx--toggle-input__label .bx--toggle__switch) {
            margin-top: 0;
        }

        :global(.autoscroll-toggle) {
            display: inline-grid;
        }

        :global(.autoscroll-toggle label) {
            display: flex;
            flex-direction: row;
            align-items: center;
            font-size: .875rem;
            margin-left: 10px;
        }

        :global(.autoscroll-toggle label :first-child) {
            margin: 8px;
        }
    }

    .settings {
        border-bottom: 1px solid lightgray;
        padding: 0 10px;
    }

    .log-list {
        height: 300px;
        width: 100%;
        border: 1px solid lightgray;
    }

    .log-line {
        display: flex;
        padding: 3px; 
    }

    .log-line.ignore  {
        text-decoration: overline red wavy;
    }

    .log-line .nr, .log-line .level, .log-line .free-heap {
        margin-right: 3px;
    }

    .log-line .nr {
        min-width: 30px;
    }

    .log-line .level {
        min-width: 39px;
    }

    .log-line .free-heap {
        min-width: 50px;
    }

    .level-COMM {
        color: blue;
    }

    .level-INFO {
        color: green;
    }

    .level-WARN {
        color: orange;
    }

    .level-ERR {
        color: red;
    }

    .level-CRIT {
        color: red;
        border: 1px solid red;
    }

    .log-line .text {
        line-break: anywhere;
    }
</style>