import { products, root, fmtUSD, stars, renderStars, renderBadges, discount, Cart, toast } from './core.js';
import { router } from './router.js';
import { productCard } from './product-card.js';

const SAMPLE_REVIEWS = [
    { name: 'Alex M.',  rating: 5, date: 'Feb 12 2026', text: 'Absolutely love it. Build quality is excellent and it arrived two days early. Highly recommend!', verified: true },
    { name: 'Sarah K.', rating: 4, date: 'Jan 28 2026', text: 'Great product overall. Setup was straightforward and performance is top notch. Knocked off one star as the packaging could be better.', verified: true },
    { name: 'James T.', rating: 5, date: 'Jan 15 2026', text: 'Best purchase I made this year. Exactly as described, and customer service was superb when I had a question.', verified: false },
];

export function renderProductDetail(id) {
    const p = products.find(item => item.id === Number(id));
    if (!p) {
        root.innerHTML = `<div class="empty-state" style="padding:8rem 2rem"><div class="ei">🔎</div><p>Product not found.</p><br><a href="/products" class="btn btn-primary" data-link>Back to shop</a></div>`;
        return;
    }

    const d        = discount(p);
    const related  = products.filter(i => i.category === p.category && i.id !== p.id).slice(0, 4);
    const stockCls = p.stock === 0 ? 'out-stock' : p.stock <= 5 ? 'low-stock' : 'in-stock';
    const stockTxt = p.stock === 0 ? '✗ Out of Stock' : p.stock <= 5 ? `⚠ Only ${p.stock} left` : '✓ In Stock';

    root.innerHTML = `
        <div class="detail-wrap">
            <div class="breadcrumb">
                <a href="/" data-link>Home</a> ›
                <a href="/products" data-link>Products</a> ›
                <a href="/products?cat=${p.category}" data-link>${p.category}</a> ›
                <span>${p.name}</span>
            </div>
            <div class="detail-grid">
                <div>
                    <div class="detail-img" id="main-img">${p.emoji}</div>
                    <div class="detail-img-thumbs">
                        ${[p.emoji, '📦', '🎁'].map((e, i) => `<div class="thumb ${i === 0 ? 'active' : ''}" data-em="${e}">${e}</div>`).join('')}
                    </div>
                </div>
                <div class="detail-info">
                    <div class="detail-badges">${renderBadges(p)}</div>
                    <h1 class="detail-name">${p.name}</h1>
                    <div class="detail-stars">
                        ${renderStars(p.rating, p.reviews)}
                        <a href="#reviews" class="reviews-link">${p.reviews.toLocaleString()} reviews</a>
                    </div>
                    <div class="detail-price-row">
                        <div class="detail-price">${fmtUSD(p.price)}</div>
                        ${d ? `<div class="detail-orig">${fmtUSD(p.originalPrice)}</div><div class="detail-save">Save ${d}%</div>` : ''}
                    </div>
                    <div class="stock-badge ${stockCls}">${stockTxt}</div>
                    <p class="detail-desc">${p.description}</p>
                    <div class="qty-control">
                        <button class="qty-btn" id="qty-dec">−</button>
                        <input class="qty-val" id="qty-val" type="number" value="1" min="1" max="${p.stock || 99}">
                        <button class="qty-btn" id="qty-inc">+</button>
                    </div>
                    <div class="detail-add-btn">
                        <button class="btn btn-primary" style="flex:1" id="atc-btn" ${p.stock === 0 ? 'disabled' : ''}>
                            ${p.stock === 0 ? 'Out of Stock' : '🛒 Add to Cart'}
                        </button>
                        <button class="btn btn-outline" id="buy-btn" ${p.stock === 0 ? 'disabled' : ''}>Buy Now</button>
                    </div>
                    <div class="specs-table">
                        <div class="specs-table-title">Product Specifications</div>
                        ${Object.entries(p.specs).map(([k, v]) => `<div class="spec-row"><div class="spec-key">${k}</div><div class="spec-val">${v}</div></div>`).join('')}
                    </div>
                </div>
            </div>

            <hr class="divider" id="reviews">
            <div class="reviews-section">
                <div class="section-hd"><div class="section-title">Customer Reviews</div></div>
                ${SAMPLE_REVIEWS.map(r => `
                    <div class="review-card">
                        <div class="review-header">
                            <div>
                                <div class="reviewer-name">${r.name}</div>
                                <div class="stars" style="font-size:.85rem">${stars(r.rating)}</div>
                            </div>
                            <div class="review-date">${r.date}</div>
                        </div>
                        <div class="review-text">${r.text}</div>
                        ${r.verified ? '<div class="review-verified">✓ Verified Purchase</div>' : ''}
                    </div>`).join('')}
            </div>

            ${related.length ? `
            <hr class="divider">
            <div class="section-hd"><div class="section-title">Related Products</div></div>
            <div class="product-grid" id="related-grid"></div>` : ''}
        </div>`;

    // Thumbnail switcher
    document.querySelectorAll('.thumb').forEach(t => {
        t.addEventListener('click', () => {
            document.querySelectorAll('.thumb').forEach(x => x.classList.remove('active'));
            t.classList.add('active');
            document.getElementById('main-img').textContent = t.dataset.em;
        });
    });

    // Qty control
    const qtyInput = document.getElementById('qty-val');
    document.getElementById('qty-dec').addEventListener('click', () => { if (qtyInput.value > 1) qtyInput.value--; });
    document.getElementById('qty-inc').addEventListener('click', () => { if (qtyInput.value < (p.stock || 99)) qtyInput.value++; });

    document.getElementById('atc-btn')?.addEventListener('click', () => {
        Cart.add(p.id, Number(qtyInput.value));
        toast(`✓ ${p.name} ×${qtyInput.value} added to cart`);
    });

    document.getElementById('buy-btn')?.addEventListener('click', () => {
        Cart.add(p.id, Number(qtyInput.value));
        router.navigate('/checkout');
    });

    // Related products
    const rg = document.getElementById('related-grid');
    if (rg) related.forEach(item => rg.appendChild(productCard(item)));
}
