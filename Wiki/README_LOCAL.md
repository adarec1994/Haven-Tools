# Dragon Age Toolset Wiki Local Mirror

Run the local mirror from the repository root:

```powershell
.\Wiki\run_wiki.ps1
```

Then open:

```text
http://127.0.0.1:8765/
```

You can pass a different port if needed:

```powershell
.\Wiki\run_wiki.ps1 8770
```

The custom server maps live-style wiki URLs such as `/wiki/2DA` and `/mw/images/...`
to the static files saved under `pages/` and `assets/`.
