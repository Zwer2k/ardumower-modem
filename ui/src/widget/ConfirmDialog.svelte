<script lang="ts">
  import {
    ComposedModal,
    ModalHeader,
    ModalBody,
    ModalFooter,
    Button,
  } from "carbon-components-svelte";
  import { confirmDialogStore, closeConfirm } from "../stores/confirm-dialog";

  let state = $state($confirmDialogStore);
  confirmDialogStore.subscribe((s) => (state = s));

  const open = $derived(!!state);
  const title = $derived(state?.title ?? "");
  const message = $derived(state?.message ?? "");
  const confirmText = $derived(state?.confirmText ?? "OK");
  const cancelText = $derived(state?.cancelText ?? "Abbrechen");
  const confirmKind = $derived(state?.kind === "danger" ? "danger" : "primary");
</script>

<ComposedModal
  {open}
  on:close={() => closeConfirm(false)}
  preventCloseOnClickOutside
>
  <ModalHeader {title} />
  <ModalBody>
    <p>{message}</p>
  </ModalBody>
  <ModalFooter>
    <Button kind="secondary" on:click={() => closeConfirm(false)}>
      {cancelText}
    </Button>
    <Button kind={confirmKind} on:click={() => closeConfirm(true)}>
      {confirmText}
    </Button>
  </ModalFooter>
</ComposedModal>
