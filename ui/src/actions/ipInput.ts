export function ipInput(node: HTMLInputElement) {
  function onInput(e: Event) {
    const target = e.target as HTMLInputElement;
    let value = target.value.replace(/[^0-9.]/g, '');
    const parts = value.split('.');
    const validParts = parts
      .map((p, i) => {
        if (i >= 4) return null;
        let num = parseInt(p, 10);
        if (isNaN(num)) return p === '' ? '' : null;
        if (num > 255) num = 255;
        return num.toString();
      })
      .filter((p): p is string => p !== null);

    let result = validParts.join('.');
    if (result.length > 15) result = result.substring(0, 15);

    if (target.value !== result) {
      target.value = result;
      target.dispatchEvent(new Event('input', { bubbles: true }));
    }
  }

  function onKeyDown(e: KeyboardEvent) {
    if (e.key === ' ') {
      e.preventDefault();
      return;
    }
    const target = e.target as HTMLInputElement;
    const value = target.value;
    const cursor = target.selectionStart ?? 0;
    if (e.key === '.' && (cursor === 0 || value[cursor - 1] === '.')) {
      e.preventDefault();
      return;
    }
    if (e.key >= '0' && e.key <= '9') {
      const partStart = value.lastIndexOf('.', cursor - 1) + 1;
      const part = value.substring(partStart, cursor) + e.key;
      if (part.length > 3) {
        e.preventDefault();
        return;
      }
    }
  }

  node.addEventListener('input', onInput);
  node.addEventListener('keydown', onKeyDown);

  return {
    destroy() {
      node.removeEventListener('input', onInput);
      node.removeEventListener('keydown', onKeyDown);
    }
  };
}
