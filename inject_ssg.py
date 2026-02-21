import sys

with open('examples/04-property-site/base_index.html', 'r') as f:
    html = f.read()

with open('examples/04-property-site/dist/App.forge.html', 'r') as f:
    prerender = f.read()

placeholder = '<forge-app></forge-app>'
if placeholder in html:
    html = html.replace(placeholder, f'<forge-app>{prerender}</forge-app>')
    with open('examples/04-property-site/index.html', 'w') as f:
        f.write(html)
    print("Successfully injected pre-rendered HTML into index.html")
else:
    print("Placeholder <forge-app></forge-app> not found")
