export const router = {
    routes:    [],
    _fallback: null,

    add(pattern, handler) { this.routes.push({ pattern, handler }); return this; },
    fallback(handler)     { this._fallback = handler; return this; },

    navigate(path) {
        history.pushState({}, '', path);
        this.resolve();
        window.scrollTo({ top: 0, behavior: 'smooth' });
    },

    resolve() {
        const path = window.location.pathname;
        for (const { pattern, handler } of this.routes) {
            const keys = [];
            const re   = pattern.replace(/:(\w+)/g, (_, k) => { keys.push(k); return '([^/?]+)'; });
            const m    = path.match(new RegExp(`^${re}$`));
            if (m) {
                const params = {};
                keys.forEach((k, i) => params[k] = m[i + 1]);
                handler(params);
                this._nav(path);
                return;
            }
        }
        if (this._fallback) this._fallback();
        this._nav('/');
    },

    _nav(path) {
        document.querySelectorAll('[data-nav]').forEach(el => {
            const h = el.getAttribute('href');
            el.classList.toggle('active',
                h === path || (h === '/products' && path.startsWith('/products'))
            );
        });
    },
};
