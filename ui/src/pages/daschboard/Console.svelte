<script lang="ts">
    import type { ConsoleLog } from "../../model";
    import { AccordionItem, ContentSwitcher, Dropdown, Switch } from "carbon-components-svelte";
    import VirtualList from "../../widget/VirtualList.svelte";

    export let logData: ConsoleLog = null;
    export let debugLevel: number = 0; 

    let items: {
        id: number;
        line: string
    }[] = [];

    let lineNr = 0;
    let autoscroll = true;

	let start = 0;
	let end = 0;
	let scrollToIndex = undefined;
    let modemLogOpen = false;
    let debugIndex = 0;
    let logLines = 10000;

    let debugLevels = [
        { id: "0", text: "none" },
        { id: "31", text: "debug" },
        { id: "15", text: "info" },
        { id: "7", text: "error" },
        { id: "3", text: "emergency" },
        { id: "1", text: "critical" },
    ];

    $: if (logData) {
        if (logData != null) {
            Object.keys(logData).forEach(key => {
                items = [...items, { id: lineNr++, line: logData[key] }];
                let remove = items.length - logLines;
                if (remove > 0)
                    items.splice(0, remove); 
            });
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

    $: { debugLevel = parseInt(debugLevels[debugIndex].id); }

</script>

<AccordionItem title="Modem Log" bind:open={modemLogOpen}>
    <div class="log-list">
        <!-- <VirtualScroll bind:this={list} data={logBuffer} let:data start={-1} on:scroll={onScroll} on:bottom={onBottom}>
            <div class="log-line"><div class="nr">{data.id}:</div><div class="text">{data.line}</div></div>
        </VirtualScroll> -->
        <div class="settings">
            <Dropdown
                bind:selectedIndex={debugIndex}
                type="inline"
                titleText="Debug level"
                items={debugLevels}/>
        </div>
        <VirtualList {items}
            height="calc(100% - 40px)"
            bind:scrollToIndex
            let:item>
            <div class="log-line"><div class="nr">{item.id}:</div><div class="text">{item.line}</div></div>
        </VirtualList>
    </div>
</AccordionItem>

<style>
    :global(.bx--accordion__item--active .bx--accordion__content) {
        display: inline;
        padding: 0;
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

    .log-line .nr {
        min-width: 30px;
        text-align: right;
        margin-right: 10px;
    }

    .log-line .text {
        line-break: anywhere;
    }
</style>