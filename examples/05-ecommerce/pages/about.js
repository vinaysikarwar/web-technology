import { root } from './core.js';

export function renderAbout() {
    root.innerHTML = `
        <section class="hero" style="text-align:center">
            <div style="max-width:680px;margin:0 auto">
                <div class="hero-eyebrow">🏪 Est. 2020</div>
                <h1 class="hero-title">About <em>ShopForge</em></h1>
                <p class="hero-sub">We started with a simple belief: shopping online should be fast, honest, and delightful.</p>
            </div>
        </section>
        <div class="section">
            <div style="display:grid;grid-template-columns:1fr 1fr;gap:4rem;align-items:center;margin-bottom:4rem">
                <div style="height:360px;background:linear-gradient(135deg,#eff0fe,#fdf4ff);border-radius:20px;display:flex;align-items:center;justify-content:center;font-size:8rem">🛍️</div>
                <div>
                    <h2 style="font-size:2rem;font-weight:800;color:var(--slate-900);margin-bottom:1rem">Our Story</h2>
                    <p style="color:var(--slate-500);line-height:1.8;margin-bottom:1.25rem">Founded in 2020, ShopForge set out to prove that ecommerce doesn't need to be slow or complicated. We built our platform on the Forge WebAssembly framework — delivering sub-20ms page loads, zero layout shift, and a shopping experience that feels native.</p>
                    <p style="color:var(--slate-500);line-height:1.8;margin-bottom:1.5rem">Today we stock over 20,000 products across five categories, partner with 300+ brands, and ship to 80 countries worldwide.</p>
                    <div style="display:grid;grid-template-columns:1fr 1fr;gap:1rem">
                        ${[['150k+','Happy Customers'],['4.9★','Average Rating'],['48hr','Delivery'],['30-day','Free Returns']].map(([v, l]) => `
                            <div style="background:var(--primary-l);border-radius:12px;padding:1.25rem">
                                <div style="font-size:1.5rem;font-weight:800;color:var(--primary)">${v}</div>
                                <div style="font-size:.825rem;color:var(--slate-500);margin-top:.2rem">${l}</div>
                            </div>`).join('')}
                    </div>
                </div>
            </div>
            <div class="section-hd" style="margin-bottom:1.5rem"><div class="section-title">Our Commitments</div></div>
            <div style="display:grid;grid-template-columns:repeat(3,1fr);gap:1.5rem">
                ${[
                    ['🌱', 'Sustainable', 'We partner with eco-certified brands and offset 100% of our shipping emissions.'],
                    ['🔒', 'Secure',      'Bank-level encryption on every transaction. Your data is never sold.'],
                    ['💬', 'Transparent', 'No dark patterns. No hidden fees. What you see is what you pay.'],
                ].map(([icon, title, desc]) => `
                    <div style="background:var(--white);border-radius:var(--radius);padding:2rem;box-shadow:var(--shadow)">
                        <div style="font-size:2rem;margin-bottom:.875rem">${icon}</div>
                        <div style="font-size:1.05rem;font-weight:700;color:var(--slate-900);margin-bottom:.5rem">${title}</div>
                        <div style="color:var(--slate-500);font-size:.875rem;line-height:1.6">${desc}</div>
                    </div>`).join('')}
            </div>
        </div>`;
}
