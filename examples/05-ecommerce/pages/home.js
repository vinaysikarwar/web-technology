import { products, CATEGORIES, root } from './core.js';
import { router } from './router.js';
import { productCard } from './product-card.js';

export function renderHome() {
    const featured = products.filter(p => p.featured);

    root.innerHTML = `
        <section class="hero">
            <div class="hero-content" style="max-width:720px;margin:0 auto">
                <div class="hero-eyebrow">⚡ Powered by Forge WebAssembly</div>
                <h1 class="hero-title">Shop <em>Smarter</em>,<br>Live Better</h1>
                <p class="hero-sub">Curated products from the world's best brands. Free shipping on orders over $50. 30-day returns.</p>
                <div class="hero-actions">
                    <button class="btn btn-primary" id="home-shop">Browse Products</button>
                    <button class="btn btn-outline" id="home-sale">View Sale Items</button>
                </div>
            </div>
        </section>

        <div class="stats-bar">
            <div class="stats-inner">
                <div><div class="stat-val">20,000+</div><div class="stat-lbl">Products</div></div>
                <div><div class="stat-val">150k+</div><div class="stat-lbl">Happy Customers</div></div>
                <div><div class="stat-val">4.9★</div><div class="stat-lbl">Average Rating</div></div>
                <div><div class="stat-val">48hr</div><div class="stat-lbl">Average Delivery</div></div>
            </div>
        </div>

        <div class="section">
            <div class="section-hd">
                <div><div class="section-title">Shop by Category</div></div>
            </div>
            <div class="cat-grid" id="cat-grid"></div>
        </div>

        <div class="section" style="padding-top:0">
            <div class="section-hd">
                <div>
                    <div class="section-title">Featured Products</div>
                    <div class="section-sub">Handpicked for quality and value</div>
                </div>
                <a href="/products" class="view-all" data-link>View all →</a>
            </div>
            <div class="product-grid" id="featured-grid"></div>
        </div>

        <div class="section" style="padding-top:0">
            <div class="promo-banner">
                <div>
                    <div class="promo-title">🏷️ Up to 30% Off Sale</div>
                    <div class="promo-sub">Limited time offers across Electronics, Clothing & more</div>
                </div>
                <button class="btn btn-white" id="promo-sale-btn">Shop the Sale</button>
            </div>
        </div>

        <div class="section" style="padding-top:0">
            <div class="section-hd">
                <div>
                    <div class="section-title">New Arrivals</div>
                    <div class="section-sub">Just landed in our store</div>
                </div>
                <a href="/products?tag=new" class="view-all" data-link>See all new →</a>
            </div>
            <div class="product-grid" id="new-grid"></div>
        </div>`;

    // Category chips
    const catGrid = document.getElementById('cat-grid');
    CATEGORIES.filter(c => c.id !== 'all').forEach(c => {
        const chip = document.createElement('a');
        chip.className    = 'cat-chip';
        chip.href         = `/products?cat=${c.id}`;
        chip.dataset.link = '';
        chip.innerHTML    = `<span class="cat-chip-icon">${c.icon}</span>${c.label}`;
        catGrid.appendChild(chip);
    });

    // Featured products
    const fg = document.getElementById('featured-grid');
    if (featured.length) featured.forEach(p => fg.appendChild(productCard(p)));
    else fg.innerHTML = `<div class="empty-state"><div class="ei">⏳</div>Loading…</div>`;

    // New arrivals
    const newProds = products.filter(p => p.tags.includes('new')).slice(0, 4);
    const ng       = document.getElementById('new-grid');
    if (newProds.length) newProds.forEach(p => ng.appendChild(productCard(p)));
    else ng.innerHTML = `<div class="empty-state"><div class="ei">📦</div>No new items</div>`;

    document.getElementById('home-shop')?.addEventListener('click',      () => router.navigate('/products'));
    document.getElementById('home-sale')?.addEventListener('click',      () => router.navigate('/products?tag=sale'));
    document.getElementById('promo-sale-btn')?.addEventListener('click', () => router.navigate('/products?tag=sale'));
}
