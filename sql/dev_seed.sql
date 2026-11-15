SET NAMES utf8mb4;

-- Development data only. Do not load in production: production uses the
-- initial inventory provided by the professor.
INSERT INTO users (username, email, password_hash, status) VALUES
    ('arlequin', 'arlequin@example.test', 'not-a-real-hash', 'active'),
    ('colombina', 'colombina@example.test', 'not-a-real-hash', 'active');

INSERT INTO objects (name, description, owner_id) VALUES
    ('Cascabel de plata', 'Cascabel ceremonial del primer bufón de la corte.', 1),
    ('Máscara bicolor', 'Máscara de media cara, mitad carmesí y mitad dorada.', 2),
    ('Naipe del comodín', 'Una carta que no pertenece a ningún palo.', NULL),
    ('Bastón con cabeza', 'Marotte tallada con una réplica en miniatura de su dueño original, con campanillas en el cuello y una cinta descolorida por los años de función en la plaza.', NULL),
    ('<script>alert(1)</script>', 'Prueba de escape: este nombre no debe ejecutarse.', 1);
