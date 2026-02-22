import { Cart, root, fmtUSD, toast, updateCartBadge } from './core.js';
import { router } from './router.js';

export function renderCheckout() {
    if (!Cart.count()) { router.navigate('/cart'); return; }

    const items = Cart.items();
    const sub   = Cart.subtotal();
    const tax   = sub * 0.08;
    const total = sub + tax;

    root.innerHTML = `
        <div class="section">
            <div class="section-hd" style="margin-bottom:1.5rem">
                <div class="section-title">Checkout</div>
            </div>
            <div class="checkout-grid">
                <div id="checkout-form-wrap">
                    <div class="checkout-form">
                        <div class="checkout-step-title">📦 Shipping Information</div>
                        <div class="form-grid-2">
                            <div class="form-group"><label class="form-label">First Name *</label><input class="form-input" id="f-first" placeholder="John"></div>
                            <div class="form-group"><label class="form-label">Last Name *</label><input class="form-input" id="f-last" placeholder="Smith"></div>
                        </div>
                        <div class="form-group"><label class="form-label">Email *</label><input class="form-input" id="f-email" type="email" placeholder="john@example.com"></div>
                        <div class="form-group"><label class="form-label">Phone</label><input class="form-input" id="f-phone" type="tel" placeholder="+1 (555) 000-0000"></div>
                        <div class="form-group"><label class="form-label">Address *</label><input class="form-input" id="f-addr" placeholder="123 Main St"></div>
                        <div class="form-grid-2">
                            <div class="form-group"><label class="form-label">City *</label><input class="form-input" id="f-city" placeholder="New York"></div>
                            <div class="form-group"><label class="form-label">ZIP Code *</label><input class="form-input" id="f-zip" placeholder="10001"></div>
                        </div>
                    </div>
                    <div class="checkout-form" style="margin-top:1rem">
                        <div class="checkout-step-title">💳 Payment</div>
                        <div class="form-group"><label class="form-label">Card Number *</label><input class="form-input" id="f-card" placeholder="4242 4242 4242 4242" maxlength="19"></div>
                        <div class="form-grid-2">
                            <div class="form-group"><label class="form-label">Expiry *</label><input class="form-input" id="f-exp" placeholder="MM/YY" maxlength="5"></div>
                            <div class="form-group"><label class="form-label">CVV *</label><input class="form-input" id="f-cvv" placeholder="•••" maxlength="4"></div>
                        </div>
                        <button class="btn btn-success btn-full" id="place-order-btn" style="margin-top:.5rem">
                            🔒 Place Order — ${fmtUSD(total)}
                        </button>
                        <div style="text-align:center;margin-top:.75rem;font-size:.78rem;color:var(--slate-400)">
                            Your payment info is encrypted and never stored.
                        </div>
                    </div>
                </div>
                <div class="order-summary" style="position:sticky;top:calc(var(--nav-h) + 1rem)">
                    <div class="summary-title">Your Order (${Cart.count()} items)</div>
                    ${items.map(({ product: p, qty }) => `
                        <div style="display:flex;gap:.875rem;align-items:center;padding:.75rem 0;border-bottom:1px solid var(--slate-100)">
                            <div style="font-size:2rem;flex-shrink:0">${p.emoji}</div>
                            <div style="flex:1;min-width:0">
                                <div style="font-size:.85rem;font-weight:600;color:var(--slate-900);white-space:nowrap;overflow:hidden;text-overflow:ellipsis">${p.name}</div>
                                <div style="font-size:.78rem;color:var(--slate-400)">Qty: ${qty}</div>
                            </div>
                            <div style="font-weight:700;font-size:.9rem;flex-shrink:0">${fmtUSD(p.price * qty)}</div>
                        </div>`).join('')}
                    <hr class="summary-divider">
                    <div class="summary-row"><span>Subtotal</span><span>${fmtUSD(sub)}</span></div>
                    <div class="summary-row"><span>Shipping</span><span style="color:var(--green)">Free</span></div>
                    <div class="summary-row"><span>Tax (8%)</span><span>${fmtUSD(tax)}</span></div>
                    <hr class="summary-divider">
                    <div class="summary-total"><span>Total</span><span>${fmtUSD(total)}</span></div>
                </div>
            </div>
        </div>`;

    document.getElementById('place-order-btn').addEventListener('click', () => {
        const required = ['f-first', 'f-last', 'f-email', 'f-addr', 'f-city', 'f-zip', 'f-card', 'f-exp', 'f-cvv'];
        const missing  = required.filter(id => !document.getElementById(id)?.value.trim());
        if (missing.length) { toast('Please fill in all required fields'); return; }

        const orderNo = 'SF-' + Math.random().toString(36).slice(2, 8).toUpperCase();
        localStorage.removeItem('shopforge_cart');
        updateCartBadge();

        document.getElementById('checkout-form-wrap').innerHTML = `
            <div class="order-placed">
                <div class="order-placed-icon">🎉</div>
                <h2>Order Placed!</h2>
                <p>Thank you for your purchase. We'll email your confirmation shortly.</p>
                <div class="order-no">Order #${orderNo}</div>
                <div style="display:flex;gap:1rem;justify-content:center;flex-wrap:wrap">
                    <button class="btn btn-primary" id="op-shop">Continue Shopping</button>
                    <button class="btn btn-outline" id="op-home">Back to Home</button>
                </div>
            </div>`;

        document.getElementById('op-shop').addEventListener('click', () => router.navigate('/products'));
        document.getElementById('op-home').addEventListener('click', () => router.navigate('/'));
    });
}
