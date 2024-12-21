import bs4
import os
sites = os.listdir("html")
for i, site in enumerate(sites):
    with open("html/"+site, "r") as file:
        parsed = bs4.BeautifulSoup(file.read(), "html.parser")
        # Remove citation marks
        for e in parsed.find_all('sup', class_="reference"):
            e.decompose()
        # Get all paragraphs, while restricting other methods result in a lot of garbage text
        content = parsed.find_all('p')
        with open("text/"+site.removesuffix(".html"), "w") as out_file:
            for p in content:
                out_file.write(p.text)

    print(f"{i+1}/{len(sites)}")

