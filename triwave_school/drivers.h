/*
 * drivers.h - TRIWAVE driver data set (used by BOTH the school and terminal sketches)
 * Source: Tricycle_Drivers_Infos.xlsx
 *
 * - ID is always 3 digits, zero-padded ("23" -> "023").
 * - Put this file in the SAME folder as each sketch (copy it to both).
 *   Keep the two copies identical.
 * - Contact numbers had their leading 0 dropped in the spreadsheet, so a 0 was added back.
 * - Avoid commas and double quotes inside names/plates.
 */
#pragma once
#include <Arduino.h>

struct Driver { const char *id; const char *name; const char *plate; const char *contact; };

const Driver DRIVERS[] = {
  { "018", "Domingo Camacho", "8281WU", "09077729662" },
  { "055", "Jayvie Bobadilla", "3180TJ", "09675729010" },
  { "134", "Cristopher Deleso", "N998UG", "09075117066" },
  { "058", "Joaquin Bobadilla", "D5464AG", "09918839607" },
  { "005", "Isagani Gutierrez", "443DXQ", "09603797774" },
  { "031", "Resty Garcia", "6869VQ", "09956189602" },
  { "129", "Dustin Ron Godoy", "0216MT", "09915969000" },
  { "024", "Ryan Villena", "72612", "09606813164" },
  { "012", "Fagie De Gracia", "N249RN", "09291748376" },
  { "013", "Felix Dapog", "5447WN", "09369786112" },
  { "094", "Joseph Bobadilla", "D927GA", "09162903236" },
  { "003", "Michael Bobadilla", "D20845", "09173372357" },
  { "030", "Isaias Arellano", "01X894", "09303455899" },
  { "132", "James Mercado", "236NBJ", "09916671889" },
  { "124", "Alex Asi", "D3U196", "09362744936" },
  { "019", "Jun Bauan", "OLU176", "09289479278" },
  { "073", "Ricardo Sarmiento", "N689UG", "09926508774" },
  { "011", "Sergio Bobadilla", "D7V579", "09922093728" },
  { "057", "Roberto Arbes", "476QUV", "09386719035" },
  { "130", "Marvin Reyes", "Q4374", "09454565227" },
  { "118", "Dhondi Alda", "N311AB", "09957237245" },
  { "052", "Mark Anthony Pisig", "794ORG", "09708204282" },
  { "008", "Manuel Dapog", "VN3544", "09752003709" },
  { "061", "Jonathan Panganiban", "8670TN", "09293930167" },
  { "083", "Randy Dimaano", "1930ZJ", "09851586873" },
  { "049", "Bernie Camacho", "DKT601", "09926268859" },
  { "071", "Florante Nable", "10K189", "09293952879" },
  { "063", "Resty Berberabe", "809DL4", "09983586262" },
  { "054", "Apolinario Capio", "02948Y", "09614163389" },
  { "099", "Larry Ilagan", "03N573", "09773994247" },
  { "001", "Fernan Panganiban", "D377NN", "09482358837" },
};

// DUPLICATE IDs in the spreadsheet - give each of these a new unique ID, then move them
// up into the list above. Until then they are NOT selectable.
/*
  { "005", "Rodel Ander", "121UYF", "09602552731" },
  { "003", "Cirilo Capio", "O0Q545", "09975418244" },
*/

const size_t DRIVER_COUNT = sizeof(DRIVERS) / sizeof(DRIVERS[0]);

// Returns the driver with this exact 3-digit ID, or nullptr.
inline const Driver *findDriver(const String &id3) {
  for (size_t i = 0; i < DRIVER_COUNT; i++)
    if (id3 == DRIVERS[i].id) return &DRIVERS[i];
  return nullptr;
}
