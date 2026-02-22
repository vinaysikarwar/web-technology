import { Cart, root, fmtUSD, toast } from './core.js';
import { router } from './router.js';

export function renderCart() {
    const items = Cart.items();

    if (!items.length) {
        root.innerHTML = `
            <div class="section">
                <div class="cart-empty">
                    <div class="cart-empty-icon">🛒</div>
                    <h2>Your cart is empty</h2>
                    <p>Looks like you haven't added anything yet.</p>
                    <button class="btn btn-primary" id="cart-shop-btn">Start Shopping</button>
                </div>
            </div>`;
        document.getElementById('cart-shop-btn').addEventListener('click', () => router.navigate('/products'));
        return;
    }

    root.innerHTML = `
        <div class="section">
            <div class="section-hd" style="margin-bottom:1.5rem">
                <div class="section-title">Shopping Cart <span style="font-weight:500;font-size:1rem;color:var(--slate-400)">(${Cart.count()} items)</span></div>
            </div>
            <div class="cart-layout">
                <div>
                    <div class="cart-table" id="cart-table"></div>
                    <div style="margin-top:1rem">
                        <a href="/products" class="btn btn-ghost btn-sm" data-link>← Continue Shopping</a>
                    </div>
                </div>
                <div class="order-summary">
                    <div class="summary-title">Order Summary</div>
                    <div class="summary-row"><span>Subtotal</span><span id="sum-sub">${fmtUSD(Cart.subtotal())}</span></div>
                    <div class="summary-row"><span>Shipping</span><span style="color:var(--green)">Free</span></div>
                    <div class="summary-row"><span>Tax (8%)</span><span id="sum-tax">${fmtUSD(Cart.subtotal() * 0.08)}</span></div>
                    <hr class="summary-divider">
                    <div class="summary-total"><span>Total</span><span id="sum-total">${fmtUSD(Cart.subtotal() * 1.08)}</span></div>
                    <div class="promo-input">
                        <input type="text" placeholder="Promo code" id="promo-in">
                        <button class="btn btn-outline btn-sm" id="promo-apply-btn">Apply</button>
                    </div>
                    <button class="btn btn-primary btn-full" id="checkout-btn">Proceed to Checkout →</button>
                    <div style="text-align:center;margin-top:1rem;font-size:.8rem;color:var(--slate-400)">🔒 Secure checkout · Free returns</div>
                </div>
            </div>
        </div>`;

    function rebuildTable() {
        const table = document.getElementById('cart-table');
        table.innerHTML = '';
        Cart.items().forEach(({ id, qty, product: p }) => {
            const row = document.createElement('div');
            row.className = 'cart-row';
            row.innerHTML = `
                <div class="cart-img">${p.emoji}</div>
                <div>
                    <div class="cart-name" style="cursor:pointer" data-go="/products/${p.id}">${p.name}</div>
                    <div class="cart-cat">${p.category}</div>
                </div>
                <div class="cart-price">${fmtUSD(p.price)}</div>
                <div class="qty-control" style="transform:scale(.85);transform-origin:left">
                    <button class="qty-btn" data-dec="${id}">−</button>
                    <input class="qty-val" type="number" value="${qty}" min="1" max="99" data-qty="${id}">
                    <button class="qty-btn" data-inc="${id}">+</button>
                </div>
                <button class="cart-remove" data-rm="${id}" title="Remove">✕</button>`;

            row.querySelector('[data-go]')?.addEventListener('click', e => router.navigate(e.currentTarget.dataset.go));
            row.querySelector('[data-dec]').addEventListener('click', () => { if (qty > 1) { Cart.setQty(id, qty - 1); rebuildTable(); recalc(); } });
            row.querySelector('[data-inc]').addEventListener('click', () => { Cart.setQty(id, qty + 1); rebuildTable(); recalc(); });
            row.querySelector('[data-qty]').addEventListener('change', e => { Cart.setQty(id, Math.max(1, Number(e.target.value))); recalc(); });
            row.querySelector('[data-rm]').addEventListener('click',  () => { Cart.remove(id); toast(`Removed ${p.name} from cart`, ''); rebuildTable(); recalc(); });
            table.appendChild(row);
        });
    }

    function recalc() {
        const sub = Cart.subtotal();
        const el  = s => document.getElementById(s);
        if (el('sum-sub'))   el('sum-sub').textContent   = fmtUSD(sub);
        if (el('sum-tax'))   el('sum-tax').textContent   = fmtUSD(sub * 0.08);
        if (el('sum-total')) el('sum-total').textContent = fmtUSD(sub * 1.08);
        if (!Cart.count()) renderCart();
    }

    rebuildTable();

    document.getElementById('checkout-btn')?.addEventListener('click', () => router.navigate('/checkout'));
    document.getElementById('promo-apply-btn')?.addEventListener('click', () => {
        const code = document.getElementById('promo-in').value.toUpperCase();
        toast(
            code === 'FORGE10' ? '✓ 10% discount applied!' : `Code "${code}" is not valid`,
            code === 'FORGE10' ? 'success' : ''
        );
    });
}
