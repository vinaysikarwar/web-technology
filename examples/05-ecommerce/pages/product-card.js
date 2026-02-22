import { discount, renderBadges, renderStars, fmtUSD, Cart, toast } from './core.js';
import { router } from './router.js';

export function productCard(p) {
    const d    = discount(p);
    const wrap = document.createElement('div');
    wrap.className = 'product-card';
    wrap.innerHTML = `
        <div class="product-card-img" data-go="/products/${p.id}">
            ${p.emoji}
            <div class="product-badges">${renderBadges(p)}</div>
        </div>
        <div class="product-card-body">
            <div class="product-cat">${p.category}</div>
            <div class="product-name" data-go="/products/${p.id}">${p.name}</div>
            ${renderStars(p.rating, p.reviews)}
            <div class="product-price-row">
                <div class="product-price">${fmtUSD(p.price)}</div>
                ${d ? `<div class="product-orig">${fmtUSD(p.originalPrice)}</div><div class="product-discount">−${d}%</div>` : ''}
            </div>
            ${p.stock <= 5 ? `<div class="stock-low">⚠️ Only ${p.stock} left</div>` : ''}
            <div class="product-card-footer">
                <button class="add-to-cart-btn" data-id="${p.id}">Add to Cart</button>
            </div>
        </div>`;

    wrap.querySelectorAll('[data-go]').forEach(el =>
        el.addEventListener('click', () => router.navigate(el.dataset.go))
    );
    wrap.querySelector('.add-to-cart-btn').addEventListener('click', e => {
        e.stopPropagation();
        Cart.add(p.id);
        toast(`✓ ${p.name} added to cart`);
        const btn = e.currentTarget;
        btn.textContent = '✓ Added';
        btn.classList.add('added');
        setTimeout(() => { btn.textContent = 'Add to Cart'; btn.classList.remove('added'); }, 1500);
    });

    return wrap;
}
