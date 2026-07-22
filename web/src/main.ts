import { createElement } from "react";
import { createRoot } from "react-dom/client";
import App from "./react/App";
import "./app.css";

const root = document.querySelector<HTMLDivElement>("#app");
if (!root) throw new Error("Application root is missing");

createRoot(root).render(createElement(App));
