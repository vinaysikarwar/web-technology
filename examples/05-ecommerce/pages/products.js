import { products, CATEGORIES, root } from './core.js';
import { router } from './router.js';
import { productCard } from './product-card.js';

export function renderProducts() {
    const sp   = new URLSearchParams(window.location.search);
    let cat    = sp.get('cat')  || 'all';
    let tag    = sp.get('tag')  || '';
    let sort   = sp.get('sort') || 'default';
    let search = sp.get('q')   || '';

    root.innerHTML = `
        <div class="section">
            <div class="section-hd">
                <div>
                    <div class="section-title">All Products</div>
                    <div class="section-sub" id="result-count"></div>
                </div>
                <div style="display:flex;gap:0.75rem;align-items:center;flex-wrap:wrap">
                    ${search ? `<span style="font-size:.875rem;color:var(--slate-500)">Results for "<strong>${search}</strong>" <a href="/products" data-link style="color:var(--primary);margin-left:.3rem">✕ Clear</a></span>` : ''}
                    <select class="sort-select" id="sort-sel">
                        <option value="default"    ${sort === 'default'    ? 'selected' : ''}>Sort: Featured</option>
                        <option value="price-asc"  ${sort === 'price-asc'  ? 'selected' : ''}>Price: Low → High</option>
                        <option value="price-desc" ${sort === 'price-desc' ? 'selected' : ''}>Price: High → Low</option>
                        <option value="rating"     ${sort === 'rating'     ? 'selected' : ''}>Top Rated</option>
                        <option value="reviews"    ${sort === 'reviews'    ? 'selected' : ''}>Most Reviewed</option>
                    </select>
                </div>
            </div>
            <div style="display:grid;grid-template-columns:220px 1fr;gap:2rem;align-items:start">
                <div>
                    <div style="font-weight:700;font-size:.875rem;color:var(--slate-700);margin-bottom:.875rem">Category</div>
                    <div class="filter-tabs" style="flex-direction:column;background:none;padding:0;gap:.25rem" id="cat-tabs"></div>
                    <hr class="divider" style="margin:1.25rem 0">
                    <div style="font-weight:700;font-size:.875rem;color:var(--slate-700);margin-bottom:.875rem">Filter by</div>
                    <div class="filter-tabs" style="flex-direction:column;background:none;padding:0;gap:.25rem" id="tag-tabs">
                        <button class="filter-tab ${!tag ? 'active' : ''}" data-tag="">All items</button>
                        <button class="filter-tab ${tag === 'sale'       ? 'active' : ''}" data-tag="sale">🏷️ On Sale</button>
                        <button class="filter-tab ${tag === 'new'        ? 'active' : ''}" data-tag="new">✨ New Arrivals</button>
                        <button class="filter-tab ${tag === 'bestseller' ? 'active' : ''}" data-tag="bestseller">🔥 Best Sellers</button>
                    </div>
                </div>
                <div>
                    <div class="product-grid" id="prod-grid"></div>
                </div>
            </div>
        </div>`;

    // Category tabs
    const catTabs = document.getElementById('cat-tabs');
    CATEGORIES.forEach(c => {
        const btn = document.createElement('button');
        btn.className    = `filter-tab ${cat === c.id ? 'active' : ''}`;
        btn.dataset.cat  = c.id;
        btn.textContent  = `${c.icon} ${c.label}`;
        btn.addEventListener('click', () => {
            cat = c.id;
            document.querySelectorAll('#cat-tabs .filter-tab').forEach(b => b.classList.toggle('active', b.dataset.cat === cat));
            applyFilters();
            updateURL();
        });
        catTabs.appendChild(btn);
    });

    // Tag tabs
    document.querySelectorAll('#tag-tabs .filter-tab').forEach(btn => {
        btn.addEventListener('click', () => {
            tag = btn.dataset.tag;
            document.querySelectorAll('#tag-tabs .filter-tab').forEach(b => b.classList.toggle('active', b.dataset.tag === tag));
            applyFilters();
            updateURL();
        });
    });

    document.getElementById('sort-sel').addEventListener('change', e => {
        sort = e.target.value;
        applyFilters();
        updateURL();
    });

    function updateURL() {
        const p = new URLSearchParams();
        if (cat  !== 'all')     p.set('cat', cat);
        if (tag)                p.set('tag', tag);
        if (sort !== 'default') p.set('sort', sort);
        if (search)             p.set('q', search);
        history.replaceState({}, '', '/products' + (p.toString() ? '?' + p.toString() : ''));
    }

    function applyFilters() {
        let list = [...products];
        if (cat !== 'all') list = list.filter(p => p.category === cat);
        if (tag)           list = list.filter(p => p.tags.includes(tag));
        if (search)        list = list.filter(p =>
            p.name.toLowerCase().includes(search.toLowerCase()) ||
            p.description.toLowerCase().includes(search.toLowerCase())
        );
        if (sort === 'price-asc')  list.sort((a, b) => a.price - b.price);
        if (sort === 'price-desc') list.sort((a, b) => b.price - a.price);
        if (sort === 'rating')     list.sort((a, b) => b.rating - a.rating);
        if (sort === 'reviews')    list.sort((a, b) => b.reviews - a.reviews);

        const grid = document.getElementById('prod-grid');
        const rc   = document.getElementById('result-count');
        if (!grid) return;
        grid.innerHTML = '';
        if (rc) rc.textContent = `${list.length} product${list.length !== 1 ? 's' : ''} found`;
        if (list.length) list.forEach(p => grid.appendChild(productCard(p)));
        else grid.innerHTML = `<div class="empty-state" style="grid-column:1/-1"><div class="ei">🔎</div>No products match your filters.</div>`;
    }

    applyFilters();
}
