<script lang="ts">
    import type { ConsoleLog } from "../../model";
    import { AccordionItem } from "carbon-components-svelte";
    import VirtualList from "../../widget/VirtualList.svelte";

    export let logData: ConsoleLog = null;
    
    let items: {
        id: number;
        line: string
    }[] = [];

    let lineNr = 0;
    let autoscroll = true;

	let start = 0;
	let end = 0;
	let scrollToIndex = undefined;
    
    $: if (logData) {
        if (logData != null) {
            Object.keys(logData).forEach(key => {
                items = [...items, { id: lineNr++, line: logData[key] }];
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
</script>

<AccordionItem title="Modem Log" open={true}>
    <div class="log-list">
        <!-- <VirtualScroll bind:this={list} data={logBuffer} let:data start={-1} on:scroll={onScroll} on:bottom={onBottom}>
            <div class="log-line"><div class="nr">{data.id}:</div><div class="text">{data.line}</div></div>
        </VirtualScroll> -->
        <VirtualList {items}
            bind:scrollToIndex
            let:item>
            <div class="log-line"><div class="nr">{item.id}:</div><div class="text">{item.line}</div></div>
        </VirtualList>
    </div>
</AccordionItem>

<style>
    :global(.bx--accordion__item--active .bx--accordion__content) {
        display: inline;
    }

    .log-list {
        height: 300px;
        width: 100%;
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