# JavaScript language extensions

Create an `*.js` file in PunPun IDE's config `extensions` directory and set one object:

```js
globalThis.punpunIdeExtension = {
  name: "My keywords",
  languages: {
    punpun: {
      keywords: ["my_keyword"],
      snippets: { "myfn": "fn myfn() {\n}" }
    }
  }
};
```

The current extension host deliberately does **not** expose filesystem, network, process or native objects to JavaScript. Extensions contribute editor metadata without becoming arbitrary-code package installers.
