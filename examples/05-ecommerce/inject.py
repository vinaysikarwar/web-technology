"""
inject.py — SSG build step for the ShopForge ecommerce example.

Reads:
  - examples/05-ecommerce/base_index.html   (template)
  - examples/05-ecommerce/api/products.json  (product catalogue)

Writes:
  - examples/05-ecommerce/index.html  (final HTML with inlined product data)

Run:
  python3 examples/05-ecommerce/inject.py
"""

import json, os

BASE = os.path.join(os.path.dirname(__file__), 'base_index.html')
DATA = os.path.join(os.path.dirname(__file__), 'api', 'products.json')
OUT  = os.path.join(os.path.dirname(__file__), 'index.html')

# 1. Read template
with open(BASE, 'r') as f:
    html = f.read()

# 2. Read product catalogue
with open(DATA, 'r') as f:
    products = json.load(f)

# 3. Build inlined <script type="application/json"> block
block = (
    '<script id="shopforge-products-data" type="application/json">\n'
    + json.dumps(products, indent=4)
    + '\n</script>'
)

placeholder = '<!-- PRODUCTS_DATA_PLACEHOLDER -->'
if placeholder in html:
    html = html.replace(placeholder, block)
else:
    print('WARNING: PRODUCTS_DATA_PLACEHOLDER not found — data not injected')

# 4. Write final index.html
with open(OUT, 'w') as f:
    f.write(html)

print(f'✓ SSG build complete → {OUT}')
print(f'  Inlined {len(products)} products from {DATA}')
