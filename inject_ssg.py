"""
inject_ssg.py — Forge SSG build step for the property site.

Reads:
  - examples/04-property-site/base_index.html   (template)
  - examples/04-property-site/dist/App.forge.html (pre-rendered component HTML)
  - examples/04-property-site/api/properties.json (API response data)

Writes:
  - examples/04-property-site/index.html  (final HTML with SSG content + inlined data)

Run after `make examples`:
  python inject_ssg.py
"""

import json

BASE   = 'examples/04-property-site/base_index.html'
PRE    = 'examples/04-property-site/dist/App.forge.html'
DATA   = 'examples/04-property-site/api/properties.json'
OUT    = 'examples/04-property-site/index.html'

# 1. Read template
with open(BASE, 'r') as f:
    html = f.read()

# 2. (Optional) Inject pre-rendered component HTML into <forge-app> if present
try:
    with open(PRE, 'r') as f:
        prerender = f.read()
    app_placeholder = '<forge-app></forge-app>'
    if app_placeholder in html:
        html = html.replace(app_placeholder, f'<forge-app>{prerender}</forge-app>')
except FileNotFoundError:
    pass  # No pre-render file — skip

# 3. Read API data and inline it as a <script type="application/json"> block
#    This makes the data visible in view-source (like a server-rendered response)
with open(DATA, 'r') as f:
    properties = json.load(f)

inlined_data_block = (
    '<script id="forge-properties-data" type="application/json">\n'
    + json.dumps(properties, indent=4)
    + '\n</script>'
)

data_placeholder = '<!-- PROPERTIES_DATA_PLACEHOLDER -->'
if data_placeholder in html:
    html = html.replace(data_placeholder, inlined_data_block)
else:
    print("WARNING: PROPERTIES_DATA_PLACEHOLDER not found — inlined data not injected")

# 4. Write final index.html
with open(OUT, 'w') as f:
    f.write(html)

print(f"✓ SSG build complete → {OUT}")
print(f"  Pre-rendered component HTML injected from {PRE}")
print(f"  Inlined {len(properties)} properties from {DATA}")
