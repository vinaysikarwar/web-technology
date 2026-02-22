import { root } from './core.js';

export function renderContact() {
    root.innerHTML = `
        <div class="section">
            <div style="display:grid;grid-template-columns:1fr 1.5fr;gap:4rem;align-items:start">
                <div>
                    <h2 style="font-size:2rem;font-weight:800;color:var(--slate-900);margin-bottom:1rem">Get in Touch</h2>
                    <p style="color:var(--slate-500);line-height:1.75;margin-bottom:2rem">Have a question about your order, a product, or our services? Our support team is here to help.</p>
                    ${[
                        ['📞', 'Phone',     '+1 (800) 456-7890'],
                        ['✉️', 'Email',     'support@shopforge.com'],
                        ['💬', 'Live Chat', 'Available Mon–Fri 8am–8pm EST'],
                        ['📍', 'HQ',        '500 Mission St, San Francisco CA'],
                    ].map(([icon, label, value]) => `
                        <div style="display:flex;align-items:center;gap:1rem;margin-bottom:1.25rem">
                            <div style="width:44px;height:44px;background:var(--primary-l);border-radius:10px;display:flex;align-items:center;justify-content:center;font-size:1.2rem;flex-shrink:0">${icon}</div>
                            <div>
                                <strong style="display:block;font-size:.85rem;color:var(--slate-900)">${label}</strong>
                                <span style="font-size:.875rem;color:var(--slate-500)">${value}</span>
                            </div>
                        </div>`).join('')}
                </div>
                <div style="background:var(--white);border-radius:20px;padding:2.5rem;box-shadow:var(--shadow)" id="contact-box">
                    <div style="font-size:1.25rem;font-weight:700;color:var(--slate-900);margin-bottom:.5rem">Send a Message</div>
                    <div style="color:var(--slate-500);font-size:.875rem;margin-bottom:1.75rem">We reply within 24 hours.</div>
                    <form id="contact-form">
                        <div class="form-grid-2">
                            <div class="form-group"><label class="form-label">Name *</label><input class="form-input" id="c-name" placeholder="Your name" required></div>
                            <div class="form-group"><label class="form-label">Email *</label><input class="form-input" id="c-email" type="email" placeholder="your@email.com" required></div>
                        </div>
                        <div class="form-group">
                            <label class="form-label">Subject</label>
                            <select class="form-input" id="c-sub">
                                <option>Order enquiry</option>
                                <option>Product question</option>
                                <option>Return / refund</option>
                                <option>Other</option>
                            </select>
                        </div>
                        <div class="form-group"><label class="form-label">Message *</label><textarea class="form-input" id="c-msg" rows="5" placeholder="How can we help?" required style="resize:vertical"></textarea></div>
                        <button type="submit" class="btn btn-primary btn-full">Send Message →</button>
                    </form>
                </div>
            </div>
        </div>`;

    document.getElementById('contact-form').addEventListener('submit', e => {
        e.preventDefault();
        const email = document.getElementById('c-email')?.value;
        document.getElementById('contact-box').innerHTML = `
            <div style="text-align:center;padding:3rem 1.5rem">
                <div style="font-size:4rem;margin-bottom:1rem">✅</div>
                <h3 style="font-size:1.5rem;font-weight:800;color:var(--slate-900);margin-bottom:.75rem">Message Sent!</h3>
                <p style="color:var(--slate-500)">We'll get back to you at <strong>${email}</strong> within 24 hours.</p>
            </div>`;
    });
}
