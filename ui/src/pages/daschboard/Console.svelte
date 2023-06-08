<script lang="ts">
    import { AccordionItem, Dropdown, Toggle } from "carbon-components-svelte";
    import VirtualList from "../../widget/VirtualList.svelte";
    import type { DropdownItem } from "carbon-components-svelte/types/Dropdown/Dropdown.svelte";
    import type { LogLevelDescT, LogLine } from "../../model";

    export let logLevels: LogLevelDescT;
    export let dbgLevels: DropdownItem[];
    export let logData: LogLine[];
    export let dbgLevel: number; 
    
    let items: LogLine[] = [];

    let autoscroll = true;

	let start = 0;
	let end = 0;
	let scrollToIndex = undefined;
    let modemLogOpen = false;
    let logLevelIndex = 3;
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
                scrollToIndex(items.length - 1);
            }
        }
    }

    // function onScroll() {
    //     autoscroll = list.getOffset() > list.getScrollSize() - list.getClientSize() - 10;
    // }

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

    $: { dbgLevel = parseInt(dbgLevels[logLevelIndex].id); }

</script>

<AccordionItem title="Modem Log" bind:open={modemLogOpen}>
    <div class="log-list">
        <!-- <VirtualScroll bind:this={list} data={logBuffer} let:data start={-1} on:scroll={onScroll} on:bottom={onBottom}>
            <div class="log-line"><div class="nr">{data.id}:</div><div class="text">{data.line}</div></div>
        </VirtualScroll> -->
        <div class="settings">
            <Dropdown
                bind:selectedIndex={logLevelIndex}
                multiple="{true}"
                type="inline"
                titleText="Log level"
                items={dbgLevels}/>
            <Toggle
                class="autoscroll-toggle"
                type="inline" 
                labelText="Autoscroll" 
                labelA={""}
                labelB={""}
                on:toggle={(e) => autoscroll = e.detail.toggled}/>
        </div>
        <VirtualList {items}
            logLevels={logLevels}
            height="calc(100% - 40px)"
            bind:scrollToIndex
            let:item>
            <div class="log-line {checkLineNr(item.nr) ? 'ignore' : ''}">
                <div class="nr">{item.nr}:</div>
                <div class="level level-{logLevels[item.level]}">{logLevels[item.level]}:</div>
                <div class="free-heap">{item.freeHeap}:</div>
                <div class="text">{item.text}</div>
            </div>
        </VirtualList>
    </div>
</AccordionItem>

<style>
    :global(.bx--accordion__item--active .bx--accordion__content) {
        display: inline;
        padding: 0;
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