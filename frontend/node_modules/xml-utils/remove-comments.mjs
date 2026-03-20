export default function removeComments(xml) {
  return xml.replace(/<!--[^]*-->/g, "");
}
