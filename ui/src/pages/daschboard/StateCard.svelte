<script lang="ts">
    import type { DesiredState, State, ValueDescriptions } from "../../model";
    import MowMotorIcon from "carbon-icons-svelte/lib/TropicalStormTracks24";

    export let state: State = null;
    export let desiredState: DesiredState = null;
    export let valueDescriptions: ValueDescriptions = null;
</script>

<article class="state-card">
	<h3>State</h3>
    {#if state != null}
        <div class="item-list">
            <div class="label">Battery</div><div>{state.battery_voltage}V/{state.amps}A</div>
            <div class="label">Position</div><div>{state.position.x}E {state.position.y}N</div>
            <div class="label">Satelite</div>
            <div>
                {valueDescriptions.posSolution[state.position.solution]}/{state.position.age.toFixed(1)}s/{state.position.accuracy}m 
                #{state.position.visible_satellites}/{state.position.visible_satellites_dgps}
            </div>
            <div class="label">State</div><div>{valueDescriptions.job[state.job]}</div>
        </div>
        <div class="item-list">
            <div class="label">OP</div><div>{valueDescriptions.job[desiredState.op]}</div>
            <div class="label">Speed</div><div>{desiredState.speed}</div>
        </div>
    {/if}
</article>
<article class="state-card">
	<h3>Controll</h3>
    {#if desiredState != null}
        <div class="controll-buttons">
            <button style="color: {desiredState.mower_motor_enabled ? 'red' : ''}"><MowMotorIcon /></button>
        </div>
    {/if}
</article>

<style>
	.state-card {
		width: 100%;
		border: 1px solid #aaa;
		border-radius: 2px;
		box-shadow: 2px 2px 8px rgba(0,0,0,0.1);
		padding: 1em;
        margin-bottom: 5px;
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

    .controll-buttons button {
        width: 50px;
        height: 50px;
    }
</style>
