// ── Shared state ──────────────────────────────────────────────────────────────
export let products = [];
export function setProducts(p) { products = p; }

export const CATEGORIES = [
    { id: 'all',         label: 'All',            icon: '🛍️' },
    { id: 'electronics', label: 'Electronics',    icon: '📱' },
    { id: 'clothing',    label: 'Clothing',       icon: '👕' },
    { id: 'books',       label: 'Books',          icon: '📚' },
    { id: 'home',        label: 'Home & Kitchen', icon: '🏠' },
    { id: 'sports',      label: 'Sports',         icon: '⚽' },
];

export const root = document.getElementById('page-root');

// ── Cart ───────────────────────────────────────────────────────────────────────
export const Cart = {
    _key: 'shopforge_cart',
    get()        { return JSON.parse(localStorage.getItem(this._key) || '[]'); },
    save(d)      { localStorage.setItem(this._key, JSON.stringify(d)); updateCartBadge(); },
    add(id, qty = 1) {
        const c  = this.get();
        const ex = c.find(i => i.id === id);
        if (ex) ex.qty = Math.min(ex.qty + qty, 99);
        else c.push({ id, qty });
        this.save(c);
    },
    remove(id)      { this.save(this.get().filter(i => i.id !== id)); },
    setQty(id, qty) { const c = this.get(); const it = c.find(i => i.id === id); if (it) it.qty = qty; this.save(c); },
    count()         { return this.get().reduce((n, i) => n + i.qty, 0); },
    subtotal() {
        return this.get().reduce((s, i) => {
            const p = products.find(p => p.id === i.id);
            return s + (p ? p.price * i.qty : 0);
        }, 0);
    },
    items() {
        return this.get()
            .map(i => ({ ...i, product: products.find(p => p.id === i.id) }))
            .filter(i => i.product);
    },
};

export function updateCartBadge() {
    const n     = Cart.count();
    const badge = document.getElementById('cart-badge');
    if (badge) { badge.textContent = n; badge.dataset.count = n; }
}

// ── Utilities ──────────────────────────────────────────────────────────────────
export const fmtUSD = n => new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD' }).format(n);
export const stars  = r => '★'.repeat(Math.round(r)) + '☆'.repeat(5 - Math.round(r));

export function toast(msg, type = 'success') {
    const el = document.createElement('div');
    el.className = `toast ${type}`;
    el.textContent = msg;
    document.getElementById('toast-container').appendChild(el);
    setTimeout(() => el.remove(), 2800);
}

export function renderStars(rating, count) {
    return `<div class="stars">${stars(rating)}<span class="star-count">${rating} (${count.toLocaleString()})</span></div>`;
}

export function renderBadges(p) {
    const b = [];
    if (p.tags.includes('sale') && p.originalPrice) b.push(`<span class="badge badge-sale">Sale</span>`);
    if (p.tags.includes('new'))        b.push(`<span class="badge badge-new">New</span>`);
    if (p.tags.includes('bestseller')) b.push(`<span class="badge badge-bs">Best Seller</span>`);
    return b.join('');
}

export function discount(p) {
    if (!p.originalPrice) return null;
    return Math.round((1 - p.price / p.originalPrice) * 100);
}
