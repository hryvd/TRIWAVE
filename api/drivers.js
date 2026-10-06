// GET /api/drivers -> list of registered tricycle drivers
const DRIVERS = [
  { id: "018", name: "Domingo Camacho", plate: "8281WU", contact: "09077729662" },
  { id: "055", name: "Jayvie Bobadilla", plate: "3180TJ", contact: "09675729010" },
  { id: "134", name: "Cristopher Deleso", plate: "N998UG", contact: "09075117066" },
  { id: "058", name: "Joaquin Bobadilla", plate: "D5464AG", contact: "09918839607" },
  { id: "005", name: "Isagani Gutierrez", plate: "443DXQ", contact: "09603797774" },
  { id: "031", name: "Resty Garcia", plate: "6869VQ", contact: "09956189602" },
  { id: "129", name: "Dustin Ron Godoy", plate: "0216MT", contact: "09915969000" },
  { id: "024", name: "Ryan Villena", plate: "72612", contact: "09606813164" },
  { id: "012", name: "Fagie De Gracia", plate: "N249RN", contact: "09291748376" },
  { id: "013", name: "Felix Dapog", plate: "5447WN", contact: "09369786112" },
  { id: "094", name: "Joseph Bobadilla", plate: "D927GA", contact: "09162903236" },
  { id: "003", name: "Michael Bobadilla", plate: "D20845", contact: "09173372357" },
  { id: "030", name: "Isaias Arellano", plate: "01X894", contact: "09303455899" },
  { id: "132", name: "James Mercado", plate: "236NBJ", contact: "09916671889" },
  { id: "124", name: "Alex Asi", plate: "D3U196", contact: "09362744936" },
  { id: "019", name: "Jun Bauan", plate: "OLU176", contact: "09289479278" },
  { id: "073", "name": "Ricardo Sarmiento", plate: "N689UG", contact: "09926508774" },
  { id: "011", name: "Sergio Bobadilla", plate: "D7V579", contact: "09922093728" },
  { id: "057", name: "Roberto Arbes", plate: "476QUV", contact: "09386719035" },
  { id: "130", name: "Marvin Reyes", plate: "Q4374", contact: "09454565227" },
  { id: "118", name: "Dhondi Alda", plate: "N311AB", contact: "09957237245" },
  { id: "052", name: "Mark Anthony Pisig", plate: "794ORG", contact: "09708204282" },
  { id: "008", name: "Manuel Dapog", plate: "VN3544", contact: "09752003709" },
  { id: "061", name: "Jonathan Panganiban", plate: "8670TN", contact: "09293930167" },
  { id: "083", name: "Randy Dimaano", plate: "1930ZJ", contact: "09851586873" },
  { id: "049", name: "Bernie Camacho", plate: "DKT601", contact: "09926268859" },
  { id: "071", name: "Florante Nable", plate: "10K189", contact: "09293952879" },
  { id: "063", name: "Resty Berberabe", plate: "809DL4", contact: "09983586262" },
  { id: "054", name: "Apolinario Capio", plate: "02948Y", contact: "09614163389" },
  { id: "099", name: "Larry Ilagan", plate: "03N573", contact: "09773994247" },
  { id: "001", name: "Fernan Panganiban", plate: "D377NN", contact: "09482358837" },
];

module.exports = (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Headers', 'x-api-key, Content-Type');
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
  if (req.method === 'OPTIONS') return res.status(200).end();

  res.status(200).json(DRIVERS);
};

module.exports.DRIVERS = DRIVERS;
