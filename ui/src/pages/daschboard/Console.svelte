<script lang="ts">
    import type { ConsoleLog } from "../../model";
    import VirtualScroll from 'svelte-virtual-scroll-list';
import { AccordionItem } from "carbon-components-svelte";

    export let logData: ConsoleLog = null;
    
    let logBuffer: {
        id: number;
        line: string
    }[] = [];

    let list;
    let lineNr = 0;
    let autoscroll = true;
    
    $: if (logData) {
        if (logData != null) {
            Object.keys(logData).forEach(key => {
                logBuffer = [...logBuffer, { id: lineNr++, line: logData[key] }];
            });
            if ((list != null) && (autoscroll)) {
                list.scrollToBottom();
            }
        }
    }

    function onScroll() {
        autoscroll = list.getOffset() > list.getScrollSize() - list.getClientSize() - 10;
    }

    function onBottom() {
        console.log("bottom");
    }
</script>

<AccordionItem title="Modem Log" open={true}>
    <div class="log-list">
        <VirtualScroll bind:this={list} data={logBuffer} let:data start={-1} on:scroll={onScroll} on:bottom={onBottom}>
            <div class="log-line"><div class="nr">{data.id}:</div><div class="text">{data.line}</div></div>
        </VirtualScroll>
    </div>
</AccordionItem>

<style>
    .log-list {
        height: 400px;
        width: calc(100vw - 110px);
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