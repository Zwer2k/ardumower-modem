<script lang="ts">
import type { State, ValueDescriptions } from "../../model";

    export let state: State = null;
    export let valueDescriptions: ValueDescriptions = null;
</script>

<article class="contact-card">
	<h3>State</h3>

    {#if state != null}
        <div class="item-list">
            <div class="label">Battery:</div><div>{state.battery_voltage}V/{state.amps}A</div>
            <div class="label">Position</div><div>{state.position.x}E {state.position.y}N</div>
            <div class="label">Satelite</div>
            <div>
                {valueDescriptions.posSolution[state.position.solution]}/{state.position.age.toFixed(1)}s/{state.position.accuracy}m 
                #{state.position.visible_satellites}/{state.position.visible_satellites_dgps}
            </div>
            <div class="label">State</div><div>{valueDescriptions.job[state.job]}</div>
        </div>
    {/if}
</article>

<style>
	.contact-card {
		width: 100%;
		border: 1px solid #aaa;
		border-radius: 2px;
		box-shadow: 2px 2px 8px rgba(0,0,0,0.1);
		padding: 1em;
	}

	h3 {
		padding: 0 0 0.2em 0;
		margin: 0 0 1em 0;
		border-bottom: 1px solid #ff3e00
	}

	.item-list {
		display: grid;
        grid-template-columns: minmax(20%, max-content) auto;
	}

    .label {
        font-weight: bold;
        margin-right: 10px;
    }
</style>
